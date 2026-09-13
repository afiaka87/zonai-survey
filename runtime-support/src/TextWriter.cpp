// SPDX-License-Identifier: GPL-2.0-only

// Original work Copyright (C) aquacluck and the totk-lotuskit contributors
// Source: https://github.com/aquacluck/totk-lotuskit (GPL-2.0-only).

// Modifications Copyright (C) Clay Mullis

#include "totk/ui/TextWriter.hpp"

namespace lotuskit {
    TextWriterFrame TextWriter::frame = {};

    void DebugDrawHooks::initializeDebugDrawers(const GraphicsModuleCreateArg* original) {
        static bool preflightAttempted = false;
        if (!original) {
            Logging.Log("[overlay] debug graphics refused: create arguments unavailable");
            TextWriter::assignHeap(nullptr);
            return;
        }
        if (g_graphicsPreflight) {
            if (preflightAttempted) return;
            preflightAttempted = true;
            if (!g_graphicsPreflight(TextWriter::debugDrawerInternalHeap)) {
                TextWriter::assignHeap(nullptr);
                return;
            }
        }
        GraphicsModuleCreateArg arg = {};
        arg.value0 = g_primitiveUniformBufferBytes != 0
            ? static_cast<s32>(g_primitiveUniformBufferBytes) : original->value0;
        arg.value1 = original->value1;
        Logging.Log("[overlay] debug graphics heap=%p ring=%d", TextWriter::debugDrawerInternalHeap,
                    arg.value0);
        auto initialize = EXL_SYM_RESOLVE<InitDebugDrawers*>("agl::init_debug_drawers");
        initialize(TextWriter::debugDrawerInternalHeap, arg);
        g_graphicsReady.store(true, std::memory_order_release);
    }

    void TextWriter::appendNewDrawNode(size_t drawList_i, const char* text,
                                      TextWriterDrawCallback* fn, float scale,
                                      const sead::Color4f* color,
                                      bool centerHorizontally, float centerX) {
        if (frame.heap == nullptr) { return; }
        TextWriterDrawNode* newNode = nullptr;
        TextWriterDrawNode* cmpNode = nullptr;
        TextWriterDrawNode* node = nullptr;

        if (!nn::os::TryLockMutex(&frame.drawListLock)) { return; }

        newNode = (TextWriterDrawNode*)frame.heap->alloc(sizeof(TextWriterDrawNode));
        if (newNode == nullptr) { goto RELEASE_AND_RETURN; }
        newNode->outputText = nullptr;
        newNode->fn = fn;
        newNode->scale = scale;
        newNode->color = color ? *color : defaultColor;
        newNode->centerHorizontally = centerHorizontally;
        newNode->centerX = centerX;
        newNode->next.store(nullptr);
        if (text != nullptr) {
            auto n = strlen(text) + 1;
            newNode->outputText = (char*)frame.heap->alloc(n);

            if (newNode->outputText == nullptr) { goto RELEASE_AND_RETURN; }
            std::memcpy(newNode->outputText, text, n);
        }

        cmpNode = nullptr;
        if (frame.drawLists[drawList_i].compare_exchange_strong(cmpNode, newNode, std::memory_order_acq_rel)) { goto RELEASE_AND_RETURN; }

        node = frame.drawLists[drawList_i].load();
        while (node) {
            cmpNode = nullptr;
            if (node->next.compare_exchange_strong(cmpNode, newNode, std::memory_order_acq_rel)) { break; }

            node = cmpNode;
        }

        RELEASE_AND_RETURN:
        nn::os::UnlockMutex(&frame.drawListLock);
    }

    void TextWriter::drawFrame(TextWriterExt* writer) {
        nn::os::LockMutex(&frame.drawListLock);

        for (size_t i=0; i < TextWriterFrame::MAX_DRAWLISTS; i++) {
            TextWriterDrawNode* node = frame.drawLists[i].load();
            if (node == nullptr) { continue; }

            sead::Vector2f textPos;
            textPos.x = 2.0;
            textPos.y = 2.0;

            do {

                writer->mScale.x = totk::ui::resolveDrawScale(node->scale, writer->mScale.x);
                writer->mScale.y = totk::ui::resolveDrawScale(node->scale, writer->mScale.y);

                if (node->fn != nullptr) {
                    node->fn(writer, &textPos);
                }
                if (node->outputText != nullptr) {
                    if (node->centerHorizontally) {
                        const float width = totk::ui::monospaceTextWidth(
                            node->outputText, totk::ui::tuning::kBannerCharW,
                            writer->mScale.x);
                        textPos.x = totk::ui::centeredTextLeftX(
                            node->centerX, width);
                    }

                    writer->pprintf(textPos, node->color, "%s", node->outputText);
                }

                node = node->next.load();
            } while (node != nullptr);

        }

        nn::os::UnlockMutex(&frame.drawListLock);
    }

    void TextWriter::drawToasts(TextWriterExt* writer) {

        nn::os::LockMutex(&frame.drawListLock);
        sead::Vector2f textPos;
        writer->mScale.x = totk::ui::tuning::kDefaultDrawScale;
        writer->mScale.y = totk::ui::tuning::kDefaultDrawScale;

        const float TOAST_ANCHOR_X = 1280.0 - 320.0;
        const float TOAST_TOP_Y = 12.0;
        textPos.x = TOAST_ANCHOR_X;
        textPos.y = TOAST_TOP_Y;

        for (size_t i=0; i < TextWriter::MAX_TOASTS; i++) {
            TextWriterToastNode* node = toasts[i].load();
            if (node == nullptr) { continue; }
            if (!totk::ui::toastVisible(node->ttlFrames)) { continue; }
            if (node->fn != nullptr) {
                node->fn(writer, &textPos);
            }
            if (node->outputText != nullptr) {

                writer->pprintf(textPos, defaultColor, "%s", node->outputText);
            }
        }
        nn::os::UnlockMutex(&frame.drawListLock);
    }

    void TextWriter::resetFrame() {

        nn::os::LockMutex(&frame.drawListLock);
        for (size_t i=0; i < TextWriterFrame::MAX_DRAWLISTS; i++) {
            frame.drawLists[i].store(nullptr);
        }
        if (frame.heap != nullptr) { frame.heap->freeAll(); }

        for (size_t i=0; i < MAX_TOASTS; i++) {
            TextWriterToastNode* node = toasts[i].load();
            if (node == nullptr) { continue; }
            if (totk::ui::classifyToastAge(node->ttlFrames) == totk::ui::ToastAge::Reap) {
                toasts[i].store(nullptr);
                if (node->outputText != nullptr) {
                    debugDrawerInternalHeap->free(node->outputText);
                }
                debugDrawerInternalHeap->free(node);
                continue;
            }
            node->ttlFrames--;
        }
        nn::os::UnlockMutex(&frame.drawListLock);
    }

    TextWriterToastNode* TextWriter::appendNewToastNode(u32 ttlFrames) {
        TextWriterToastNode* newNode = (TextWriterToastNode*)debugDrawerInternalHeap->alloc(sizeof(TextWriterToastNode));
        if (newNode == nullptr) {
            totk::ui::emitDiagnostic("[totk-ui] ERROR: toast node alloc failed");
            return nullptr;
        }
        newNode->outputText = nullptr;
        newNode->fn = nullptr;
        newNode->ttlFrames = ttlFrames;

        TextWriterToastNode* cmpNode;
        for(size_t i=0; i < MAX_TOASTS; i++) {
            cmpNode = nullptr;
            if (toasts[i].compare_exchange_strong(cmpNode, newNode)) { return newNode; }
        }

        debugDrawerInternalHeap->free(newNode);
        totk::ui::emitDiagnostic("[totk-ui] ERROR: toast overflow");
        return nullptr;
    }

}
