// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include "GlyphRenderer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <common/aglDrawContext.h>
#include <gfx/seadCamera.h>
#include <gfx/seadColor.h>
#include <gfx/seadProjection.h>
#include <lyr/aglLayer.h>
#include <lyr/aglRenderInfo.h>
#include <math/seadMatrix.h>
#include <math/seadVector.h>

#include "CameraHeading.hpp"
#include "totk/render/DrawOffsets.hpp"
#include "GlyphIndex.hpp"
#include "IconGlyphs.hpp"
#include "PerfCounters.hpp"
#include "totk/ui/TextWriter.hpp"

namespace zonai_survey::render {
namespace {

GlyphFrame g_frames[2]{};
std::atomic<std::uint32_t> g_front{0};
std::atomic<bool> g_probeVisible{false};
std::atomic<std::uint32_t> g_drawn{0};
std::atomic<std::uint32_t> g_offscreen{0};
std::atomic<std::uint32_t> g_namesDropped{0};
bool g_loggedLayer = false;

constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;

constexpr float kEdgeMarginX = 24.0f;
constexpr float kEdgeMarginY = 16.0f;

constexpr float kAnchorLiftMeters = 0.9f;

constexpr float kNameGapPixels = 3.0f;

constexpr float kCharWidth = 8.0f;
constexpr float kIconWidth = 16.0f;
constexpr float kLineHeight = 20.0f;

constexpr float kDistanceDimNear = 40.0f;
constexpr float kDistanceDimFar = 300.0f;
constexpr float kDistanceDimFloor = 0.35f;

float distanceDim(float distanceSq) {
    const float metres = __builtin_sqrtf(distanceSq);
    if (metres <= kDistanceDimNear) return 1.0f;
    if (metres >= kDistanceDimFar) return kDistanceDimFloor;
    const float t = (metres - kDistanceDimNear) / (kDistanceDimFar - kDistanceDimNear);
    return 1.0f - t * (1.0f - kDistanceDimFloor);
}

struct LabelBox {
    float x0, y0, x1, y1;
};

bool boxesOverlap(const LabelBox& a, const LabelBox& b) {
    return !(a.x1 < b.x0 || b.x1 < a.x0 || a.y1 < b.y0 || b.y1 < a.y0);
}

bool usableGameplayLayer(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info) {
    if (!layer) return false;
    if (!info.camera || !info.projection) return false;
    const char* name = layer->mLayerName.cstr();
    return name && std::strcmp(name, totk::render::kGameplayLayer) == 0;
}

bool projectToScreen(const sead::Matrix34f& view, const sead::Matrix44f& proj, float wx,
                     float wy, float wz, float& outX, float& outY) {
    const float cx = view(0, 0) * wx + view(0, 1) * wy + view(0, 2) * wz + view(0, 3);
    const float cy = view(1, 0) * wx + view(1, 1) * wy + view(1, 2) * wz + view(1, 3);
    const float cz = view(2, 0) * wx + view(2, 1) * wy + view(2, 2) * wz + view(2, 3);

    const float clipX = proj(0, 0) * cx + proj(0, 1) * cy + proj(0, 2) * cz + proj(0, 3);
    const float clipY = proj(1, 0) * cx + proj(1, 1) * cy + proj(1, 2) * cz + proj(1, 3);
    const float clipW = proj(3, 0) * cx + proj(3, 1) * cy + proj(3, 2) * cz + proj(3, 3);

    if (!__builtin_isfinite(clipW) || clipW < 0.0001f) return false;

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!__builtin_isfinite(ndcX) || !__builtin_isfinite(ndcY)) return false;
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) return false;

    outX = (ndcX * 0.5f + 0.5f) * kScreenWidth;
    outY = (0.5f - ndcY * 0.5f) * kScreenHeight;  
    return true;
}

std::uint8_t iconFor(const GlyphDraw& glyph) {
    if ((glyph.flags & glyphs::kFlagInChest) != 0) {
        return static_cast<std::uint8_t>(icons::Icon::Chest);
    }
    return glyph.icon;
}

void drawIconProbe(lotuskit::TextWriterExt* writer) {
    constexpr float kRowHeight = 22.0f;
    constexpr float kColumnWidth = 168.0f;
    constexpr float kLabelIndent = 22.0f;
    constexpr float kTop = 90.0f;
    constexpr float kBottom = 640.0f;

    float y = kTop;
    float x = 40.0f;
    for (std::uint8_t i = 0; i < icons::kIconCount; ++i) {
        const pure::Tint tint = pure::glyphTint(i);

        sead::Vector2f iconPos{x, y};
        writer->pprintf(iconPos, sead::Color4f{tint.r, tint.g, tint.b, 1.0f}, "%s",
                        pure::glyphIcon(i));
        sead::Vector2f labelPos{x + kLabelIndent, y};
        writer->pprintf(labelPos, sead::Color4f{0.96f, 0.97f, 1.0f, 1.0f}, "%s",
                        icons::iconLabel(i));

        y += kRowHeight;
        if (y > kBottom) {
            y = kTop;
            x += kColumnWidth;
        }
    }
}

}  

void publishGlyphs(const GlyphFrame& frame) {
    const std::uint32_t back = 1u - g_front.load(std::memory_order_relaxed);
    g_frames[back] = frame;
    g_front.store(back, std::memory_order_release);
}

void clearGlyphs() {
    const std::uint32_t back = 1u - g_front.load(std::memory_order_relaxed);
    g_frames[back].count = 0;
    g_front.store(back, std::memory_order_release);
}

void setSymbolProbeVisible(bool visible) {
    g_probeVisible.store(visible, std::memory_order_release);
}

bool symbolProbeVisible() { return g_probeVisible.load(std::memory_order_acquire); }

std::uint32_t lastGlyphsDrawn() { return g_drawn.load(std::memory_order_acquire); }
std::uint32_t lastGlyphsOffscreen() { return g_offscreen.load(std::memory_order_acquire); }
std::uint32_t lastNamesDropped() { return g_namesDropped.load(std::memory_order_acquire); }

void drawGlyphs(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info,
                lotuskit::TextWriterExt* writer) {
    if (!writer) return;
    if (!usableGameplayLayer(layer, info)) return;

    const sead::Matrix34f& view = info.camera->getMatrix();
    const sead::Matrix44f& proj = info.projection->getDeviceProjectionMatrix();

    engine::publishCameraForward(-view(2, 0), -view(2, 1), -view(2, 2));

    if (symbolProbeVisible()) drawIconProbe(writer);

    const GlyphFrame& frame = g_frames[g_front.load(std::memory_order_acquire)];
    if (!frame.count) return;
    engine::perf::Timer glyphTimer(engine::perf::drawGlyphText);

    if (!g_loggedLayer) {
        g_loggedLayer = true;
        Logging.Log("[zonai-survey] glyph renderer reached %s glyphs=%u", totk::render::kGameplayLayer,
                    frame.count);
    }

    std::uint32_t drawn = 0;
    std::uint32_t offscreen = 0;
    std::uint32_t namesDropped = 0;

    std::uint32_t order[pure::kMaxGlyphs];
    const std::uint32_t count =
        frame.count < pure::kMaxGlyphs ? frame.count : pure::kMaxGlyphs;
    for (std::uint32_t i = 0; i < count; ++i) order[i] = i;
    for (std::uint32_t i = 1; i < count; ++i) {
        const std::uint32_t key = order[i];
        std::uint32_t j = i;
        while (j > 0 && frame.glyphs[order[j - 1]].distanceSq > frame.glyphs[key].distanceSq) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = key;
    }

    LabelBox drawnBoxes[pure::kMaxGlyphs];
    std::uint32_t boxCount = 0;

    for (std::uint32_t i = 0; i < count; ++i) {
        const GlyphDraw& glyph = frame.glyphs[order[i]];
        if (!(glyph.alpha > 0.0f)) continue;

        float sx = 0.0f;
        float sy = 0.0f;
        if (!projectToScreen(view, proj, glyph.x, glyph.y + kAnchorLiftMeters, glyph.z, sx, sy)) {
            ++offscreen;
            continue;
        }
        if (sx < kEdgeMarginX || sx > kScreenWidth - kEdgeMarginX || sy < kEdgeMarginY ||
            sy > kScreenHeight - kEdgeMarginY) {
            ++offscreen;
            continue;
        }

        const char* name = pure::glyphDisplayName(glyph.name);
        float width = kIconWidth;
        if (name) {
            std::size_t letters = 0;
            for (const char* p = name; *p; ++p) ++letters;
            width += kNameGapPixels + kCharWidth * static_cast<float>(letters);
        }

        LabelBox box{sx, sy, sx + width, sy + kLineHeight};
        if (name) {
            for (std::uint32_t b = 0; b < boxCount; ++b) {
                if (boxesOverlap(box, drawnBoxes[b])) {
                    name = nullptr;
                    ++namesDropped;
                    box = LabelBox{sx, sy, sx + kIconWidth, sy + kLineHeight};
                    break;
                }
            }
        }

        const float alpha = glyph.alpha * distanceDim(glyph.distanceSq);

        const pure::Tint tint = pure::glyphTint(iconFor(glyph));
        sead::Vector2f pos{sx, sy};
        writer->pprintf(pos, sead::Color4f{tint.r, tint.g, tint.b, alpha}, "%s",
                        pure::glyphIcon(iconFor(glyph)));
        if (name) {
            pos.x += kNameGapPixels;
            writer->pprintf(pos, sead::Color4f{0.96f, 0.97f, 1.0f, alpha}, "%s", name);
        }

        if (boxCount < pure::kMaxGlyphs) drawnBoxes[boxCount++] = box;
        ++drawn;
    }

    g_namesDropped.store(namesDropped, std::memory_order_release);
    g_drawn.store(drawn, std::memory_order_release);
    g_offscreen.store(offscreen, std::memory_order_release);
}

}  
