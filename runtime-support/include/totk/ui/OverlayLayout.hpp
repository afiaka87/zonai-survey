// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace totk::ui {

namespace tuning {

    inline constexpr float kScreenW = 1280.0f;
    inline constexpr float kScreenH = 720.0f;

    inline constexpr float  kBannerScale   = 1.40f;
    inline constexpr float  kBannerCharW   = 8.5f;
    inline constexpr float  kBannerTopY    = 16.0f;
    inline constexpr float  kBannerRightMargin = 0.0f;
    inline constexpr std::size_t kBannerBuf = 384;

    inline constexpr float  kHudScale        = 1.10f;
    inline constexpr float  kHudLineH        = 15.7f;
    inline constexpr float  kHudX            = 8.0f;
    inline constexpr float  kHudBottomMargin = 14.0f;
    inline constexpr std::size_t kHudBuf      = 2048;
    inline constexpr std::size_t kHudMaxLines = 24;
    inline constexpr std::size_t kHudLineBuf  = 224;

    inline constexpr float  kChargeScale = 0.95f;
    inline constexpr float  kChargeX     = 810.0f;
    inline constexpr float  kChargeY     = 63.0f;
    inline constexpr std::size_t kChargeBuf = 128;

    inline constexpr float kDefaultDrawScale = 0.8f;

    inline constexpr std::size_t kMaxToasts = 0x20;

    inline constexpr std::uint32_t kPrimitiveUniformBlockBytes = 256;
    inline constexpr std::uint32_t kPrimitiveUniformDefaultBytes = 0x32000;

    inline constexpr std::uint32_t kPrimitiveUniformMaxBytes = 48u * 1024u * 1024u;
}

inline bool isSanePrimitiveUniformBufferBytes(std::uint32_t bytes) {
    return bytes >= tuning::kPrimitiveUniformDefaultBytes &&
           bytes <= tuning::kPrimitiveUniformMaxBytes &&
           bytes % tuning::kPrimitiveUniformBlockBytes == 0;
}

struct Rgba {
    float r, g, b, a;
};

struct BannerPresentation {
    float scale;
    float glyphAdvance;
    float topY;
    float rightMargin;
    std::size_t bufferBytes;
    Rgba defaultColor;
};

struct HudPresentation {
    float scale;
    float lineHeight;
    float x;
    float bottomMargin;
    std::size_t bufferBytes;
    std::size_t maxLines;
    Rgba color;
};

struct OverlayPresentation {
    BannerPresentation banner;
    HudPresentation hud;
};

constexpr OverlayPresentation canonicalPresentation() {
    return OverlayPresentation{
        BannerPresentation{
            tuning::kBannerScale,
            tuning::kBannerCharW,
            tuning::kBannerTopY,
            tuning::kBannerRightMargin,
            tuning::kBannerBuf,
            Rgba{1.0f, 1.0f, 1.0f, 1.0f},
        },
        HudPresentation{
            tuning::kHudScale,
            tuning::kHudLineH,
            tuning::kHudX,
            tuning::kHudBottomMargin,
            tuning::kHudBuf,
            tuning::kHudMaxLines,
            Rgba{1.0f, 1.0f, 1.0f, 1.0f},
        },
    };
}

inline bool isSaneColor(const Rgba& c) {
    const float parts[4] = {c.r, c.g, c.b, c.a};
    for (float v : parts) {
        if (!std::isfinite(v) || v < 0.0f || v > 1.0f) { return false; }
    }
    return true;
}

inline bool isSanePresentation(const OverlayPresentation& p) {
    const BannerPresentation& b = p.banner;
    if (!std::isfinite(b.scale) || b.scale <= 0.0f || b.scale > 8.0f) { return false; }
    if (!std::isfinite(b.glyphAdvance) || b.glyphAdvance <= 0.0f || b.glyphAdvance > 64.0f) { return false; }
    if (!std::isfinite(b.topY) || b.topY < 0.0f || b.topY >= tuning::kScreenH) { return false; }
    if (!std::isfinite(b.rightMargin) || b.rightMargin < 0.0f || b.rightMargin >= tuning::kScreenW) { return false; }
    if (b.bufferBytes < 16 || b.bufferBytes > tuning::kBannerBuf) { return false; }
    if (!isSaneColor(b.defaultColor)) { return false; }

    const HudPresentation& h = p.hud;
    if (!std::isfinite(h.scale) || h.scale <= 0.0f || h.scale > 8.0f) { return false; }
    if (!std::isfinite(h.lineHeight) || h.lineHeight <= 0.0f || h.lineHeight > 64.0f) { return false; }
    if (!std::isfinite(h.x) || h.x < 0.0f || h.x >= tuning::kScreenW) { return false; }
    if (!std::isfinite(h.bottomMargin) || h.bottomMargin < 0.0f || h.bottomMargin >= tuning::kScreenH) { return false; }
    if (h.bufferBytes < 16 || h.bufferBytes > tuning::kHudBuf) { return false; }
    if (h.maxLines < 1 || h.maxLines > tuning::kHudMaxLines) { return false; }
    if (!isSaneColor(h.color)) { return false; }
    return true;
}

inline int measureLongestDisplayLine(const char* text) {
    int maxChars = 0;
    int run = 0;
    for (const char* p = text; ; ++p) {
        if (*p == '\n' || *p == '\0') {
            if (run > maxChars) { maxChars = run; }
            run = 0;
            if (*p == '\0') { break; }
        } else {
            ++run;
        }
    }
    return maxChars;
}

inline int countDisplayLines(const char* text) {
    int lines = 1;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '\n') { ++lines; }
    }
    return lines;
}

inline float bannerLeftX(int maxChars, float charW, float scale,
                         float screenW, float rightMargin) {
    const float blockW = (float)maxChars * charW * scale;
    float x = screenW - rightMargin - blockW;
    if (x < 0.0f) { x = 0.0f; }
    return x;
}

inline float centeredTextLeftX(float anchorX, float measuredWidth) {
    return anchorX - measuredWidth * 0.5f;
}

inline float monospaceTextWidth(const char* text, float glyphAdvance, float scale) {
    return static_cast<float>(measureLongestDisplayLine(text)) * glyphAdvance * scale;
}

inline float resolveDrawScale(float requested, float current) {
    if (requested == 0.0f) { return tuning::kDefaultDrawScale; }
    if (requested == -1.0f) { return current; }
    return requested;
}

inline std::size_t buildChargeMeterText(char* out, std::size_t cap,
                                        const char* friendlyName,
                                        unsigned steps, unsigned threshold,
                                        bool full) {
    if (cap == 0) { return 0; }
    if (threshold == 0) { threshold = 1; }
    if (steps > threshold) { steps = threshold; }

    char pips[16] = {};
    unsigned at = 0;
    for (unsigned i = 0; i < threshold && at + 3 < sizeof(pips); ++i) {
        pips[at++] = '[';
        pips[at++] = i < steps ? '#' : '.';
        pips[at++] = ']';
    }
    pips[at] = '\0';

    std::size_t n = 0;
    auto append = [&](const char* s) {
        while (*s != '\0' && n + 1 < cap) { out[n++] = *s++; }
    };
    append(">--");
    append(pips);
    append(" ");
    append(friendlyName != nullptr ? friendlyName : "relic");
    if (full) { append(" READY"); }
    out[n] = '\0';
    return n;
}

inline bool hudAppendLine(char* buf, std::size_t cap, const char* line) {
    std::size_t len = 0;
    while (buf[len] != '\0') { ++len; }
    if (len + 2 >= cap) { return false; }
    if (len > 0) { buf[len++] = '\n'; buf[len] = '\0'; }
    std::size_t i = 0;
    for (; line[i] != '\0' && len + i + 1 < cap; ++i) { buf[len + i] = line[i]; }
    buf[len + i] = '\0';
    return true;
}

inline std::size_t hudNextLine(const char*& cursor, char* out, std::size_t outCap) {
    std::size_t n = 0;
    while (cursor[n] != '\0' && cursor[n] != '\n' && n < outCap - 1) {
        out[n] = cursor[n];
        ++n;
    }
    out[n] = '\0';
    cursor += n;
    if (*cursor == '\n') { ++cursor; }
    return n;
}

inline void hudLineYs(float* ys, std::size_t slotCount, int lineCount,
                      float lineH, float scale, float screenH, float bottomMargin) {
    if (lineCount > (int)slotCount) { lineCount = (int)slotCount; }
    const float step = lineH * scale;
    const float top  = screenH - bottomMargin - (float)lineCount * step;
    for (std::size_t i = 0; i < slotCount; ++i) { ys[i] = top + (float)i * step; }
}

enum class ToastAge : std::uint8_t {
    Reap,
    Decrement,
};

inline bool toastVisible(std::uint32_t ttlFrames) { return ttlFrames != 0; }

inline ToastAge classifyToastAge(std::uint32_t ttlFrames) {
    return ttlFrames == 0 ? ToastAge::Reap : ToastAge::Decrement;
}

inline int firstFreeToastSlot(const bool* occupied, std::size_t slotCount) {
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (!occupied[i]) { return (int)i; }
    }
    return -1;
}

}
