// SPDX-License-Identifier: GPL-2.0-only

// Original work Copyright (C) aquacluck and the totk-lotuskit contributors
// Source: https://github.com/aquacluck/totk-lotuskit (GPL-2.0-only).

// Modifications Copyright (C) Clay Mullis

#pragma once
#include <atomic>
#include "totk/engine/SymResolve.hpp"

#include <nn/nn.h>
#include <nn/util.h>
#include <heap/seadHeap.h>
#include <heap/seadFrameHeap.h>
#include <math/seadVector.h>

#include <common/aglDrawContext.h>
#include <lyr/aglLayer.h>
#include <lyr/aglRenderInfo.h>
#include <gfx/seadColor.h>
#include <gfx/seadDrawContext.h>
#include <gfx/seadTextWriter.h>

#include "totk/ui/Diagnostics.hpp"
#include "totk/ui/OverlayLayout.hpp"

namespace lotuskit {
    class TextWriterExt;
    using TextWriterDrawCallback = void(TextWriterExt*, sead::Vector2f*);

    struct TextWriterDrawNode {

        char* outputText;
        TextWriterDrawCallback* fn;
        std::atomic<TextWriterDrawNode*> next;
        sead::Color4f color;
        float scale;
        bool centerHorizontally;
        float centerX;
    };

    struct TextWriterFrame {
        static constexpr size_t MAX_DRAWLISTS = 8;
        inline static std::atomic<TextWriterDrawNode*> drawLists[MAX_DRAWLISTS] = {0};
        inline static sead::FrameHeap* heap = nullptr;
        nn::os::MutexType drawListLock;
    };

    struct TextWriterToastNode {
        char* outputText;
        TextWriterDrawCallback* fn;
        u32 ttlFrames;
    };

    class TextWriterExt;

    class TextWriter {
        public:
        inline static sead::Color4f defaultColor = {1.0, 1.0, 1.0, 1.0};
        inline static sead::Color4f shadowColor = {0, 0, 0, 1};
        inline static void setDefaultColor(const sead::Color4f& v) { defaultColor = v; }
        inline static void setShadowColor(const sead::Color4f& v) { shadowColor = v; }
        inline static void invertColors() { std::swap(defaultColor, shadowColor); }

        inline static void printf(size_t drawList_i, const char* fmt, auto&&... args) {
            char buf[2000];
            nn::util::SNPrintf(buf, sizeof(buf), fmt, std::forward<decltype(args)>(args)...);
            appendNewDrawNode(drawList_i, buf, nullptr);
        }
        inline static void printf(size_t drawList_i, float scale, const sead::Color4f& color, const char* fmt, auto&&... args) {
            char buf[2000];
            nn::util::SNPrintf(buf, sizeof(buf), fmt, std::forward<decltype(args)>(args)...);
            appendNewDrawNode(drawList_i, buf, nullptr, scale, &color);
        }
        inline static void appendCallback(size_t drawList_i, TextWriterDrawCallback* fn) {
            appendNewDrawNode(drawList_i, nullptr, fn);
        }
        inline static void toastf(u32 ttlFrames, const char* fmt, auto&&... args) {
            TextWriterToastNode* newNode = appendNewToastNode(ttlFrames);
            if (newNode) {

                char buf[2000];
                nn::util::SNPrintf(buf, sizeof(buf), fmt, std::forward<decltype(args)>(args)...);
                size_t n = strlen(buf) + 1;
                newNode->outputText = (char*)debugDrawerInternalHeap->alloc(n);

                if (newNode->outputText != nullptr) {
                    std::memcpy(newNode->outputText, buf, n);
                }
            }
        }

        static constexpr size_t MAX_TOASTS = totk::ui::tuning::kMaxToasts;
        inline static std::atomic<TextWriterToastNode*> toasts[MAX_TOASTS] = {0};

        static TextWriterFrame frame;
        inline static sead::Heap* debugDrawerInternalHeap = nullptr;
        inline static void assignHeap(sead::Heap* heap) {

            debugDrawerInternalHeap = heap;
        }
        inline static void createFrameHeap() {

            using impl_t = sead::FrameHeap* (size_t, const sead::SafeString&, sead::Heap*, s32, sead::Heap::HeapDirection, bool);
            auto impl = EXL_SYM_RESOLVE<impl_t*>("sead::FrameHeap::create");
            frame.heap = impl(0x4000, "lotuskit::TextWriter", debugDrawerInternalHeap, 8, (sead::Heap::HeapDirection)1,   false);

            nn::os::InitializeMutex(&frame.drawListLock, true, 0);
        }
        static void appendNewDrawNode(size_t drawList_i, const char* text = nullptr,
                                      TextWriterDrawCallback* fn = nullptr,
                                      float scale = 0,
                                      const sead::Color4f* color = nullptr,
                                      bool centerHorizontally = false,
                                      float centerX = 0.0f);
        static void appendCenteredDrawNode(size_t drawList_i, const char* text,
                                           TextWriterDrawCallback* fn, float centerX,
                                           float scale = 0,
                                           const sead::Color4f* color = nullptr) {
            appendNewDrawNode(drawList_i, text, fn, scale, color, true, centerX);
        }
        static TextWriterToastNode* appendNewToastNode(u32 ttlFrames);
        static void drawFrame(TextWriterExt*);
        static void drawToasts(TextWriterExt*);
        static void resetFrame();
    };

    class TextWriterExt: public sead::TextWriter {
        public:
        void getCursorFromTopLeftImpl(sead::Vector2f* pos) const {

            pos->x = this->mCursor.x + 640.0;
            pos->y = 360.0 - this->mCursor.y;
        }
        void pprintf(sead::Vector2f &pos, const sead::Color4f& color, const char* fmt, auto&&... args) {

            this->mColor = lotuskit::TextWriter::shadowColor;
            this->setCursorFromTopLeft(pos);
            this->printf(fmt, std::forward<decltype(args)>(args)...);

            this->mColor = color;
            pos.x -= 1.0;
            pos.y -= 1.0;
            this->setCursorFromTopLeft(pos);
            this->printf(fmt, std::forward<decltype(args)>(args)...);

            this->getCursorFromTopLeftImpl(&pos);
            pos.x += 1.0;
            pos.y += 1.0;
        }
    };

    namespace DebugDrawHooks {

        using WorldDrawCallback = void(*)(agl::lyr::Layer*, const agl::lyr::RenderInfo&,
                                          TextWriterExt*);

        using NativeWorldDrawCallback = void(*)(agl::lyr::Layer*,
                                                const agl::lyr::RenderInfo&);
        inline WorldDrawCallback g_worldDrawCallback = nullptr;
        inline NativeWorldDrawCallback g_nativeWorldDrawCallback = nullptr;
        inline std::atomic<bool> g_textVisible = true;
        inline std::uint32_t g_primitiveUniformBufferBytes = 0;
        using GraphicsPreflight = bool(*)(sead::Heap*);
        inline GraphicsPreflight g_graphicsPreflight = nullptr;
        inline std::atomic<bool> g_graphicsReady{false};
        inline void setGraphicsPreflight(GraphicsPreflight callback) {
            g_graphicsPreflight = callback;
        }
        inline bool graphicsReady() {
            return g_graphicsReady.load(std::memory_order_acquire);
        }
        inline void setWorldDrawCallback(WorldDrawCallback callback) {
            g_worldDrawCallback = callback;
        }
        inline void setNativeWorldDrawCallback(NativeWorldDrawCallback callback) {
            g_nativeWorldDrawCallback = callback;
        }
        inline void setTextVisible(bool visible) {
            g_textVisible.store(visible, std::memory_order_release);
        }
        inline bool textVisible() {
            return g_textVisible.load(std::memory_order_acquire);
        }
        inline void setPrimitiveUniformBufferBytes(std::uint32_t bytes) {
            g_primitiveUniformBufferBytes = bytes;
        }

        struct GraphicsModuleCreateArg {
            u8 _whatever[0xb4c];
            s32 value0;
            u8 _whatever2[0x10];
            s32 value1;
        };
        using InitDebugDrawers = void(sead::Heap*, GraphicsModuleCreateArg&);
        void initializeDebugDrawers(const GraphicsModuleCreateArg* original);

        HOOK_DEFINE_INLINE(BootupInitDebugDrawersHook) {
            static constexpr auto s_name = "agl::create_arg";
            static void Callback(exl::hook::InlineCtx* ctx) {
                initializeDebugDrawers(reinterpret_cast<GraphicsModuleCreateArg*>(ctx->X[1]));
            }
        };

        HOOK_DEFINE_INLINE(DebugDrawLayerMaskHook) {
            static constexpr auto s_name = "agl::lyr::RenderDisplay::drawLayer_::ensure_font";
            static void Callback(exl::hook::InlineCtx* ctx) {

                auto* layer = (agl::lyr::Layer*)(ctx->X[21]);
                ctx->W[8] = 0x28;
                layer->mRenderFlags |= 1 << 13;
            }
        };

        HOOK_DEFINE_TRAMPOLINE(DebugDrawHook) {
            static constexpr auto s_name = "agl::lyr::Layer::drawDebugInfo_";
            static void Callback(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info) {
                auto* sead_draw_ctx = dynamic_cast<sead::DrawContext*>(info.draw_ctx);
                if (sead_draw_ctx == nullptr) { return; }

                if (g_nativeWorldDrawCallback != nullptr) {
                    g_nativeWorldDrawCallback(layer, info);
                }

                if (lotuskit::TextWriter::frame.heap == nullptr) { return; }
                if (!textVisible()) { return; }

                sead::TextWriter::setupGraphics(sead_draw_ctx);
                sead::TextWriter _writer(sead_draw_ctx, info.viewport);
                TextWriterExt* writer = (TextWriterExt*)(&_writer);
                if (g_worldDrawCallback != nullptr) {
                    g_worldDrawCallback(layer, info, writer);
                }
                lotuskit::TextWriter::drawFrame(writer);
                lotuskit::TextWriter::drawToasts(writer);
            }
        };

    }

}
