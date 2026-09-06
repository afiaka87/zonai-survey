// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis; StealHeapHook derived from totk-lotuskit by aquacluck and contributors.

#include <cstring>

#include "totk/ui/Overlay.hpp"       
#include "totk/ui/OverlayLayout.hpp"
#include "totk/ui/Diagnostics.hpp"

#if OVERLAY_DEBUG_HUD
#include <cstdarg>
#include <array>       
#include <utility>     
#endif

namespace overlay {

namespace tuning = totk::ui::tuning;

namespace {
    totk::ui::OverlayPresentation g_pres = totk::ui::canonicalPresentation();
    bool g_presLocked = false;

    inline sead::Color4f toSead(const totk::ui::Rgba& c) {
        return sead::Color4f{c.r, c.g, c.b, c.a};
    }
    constexpr size_t BANNER_DRAWLIST = 0;
    constexpr size_t BANNER_BUF      = tuning::kBannerBuf;

    char          g_bannerText[BANNER_BUF] = {};
    int           g_bannerTtlTicks         = 0;
    float         g_bannerX                = 0.0f;   
    float         g_bannerY                = 0.0f;
    sead::Color4f g_bannerColor            = {1.0f, 1.0f, 1.0f, 1.0f};
    bool          g_gfxInited              = false;  

    constexpr size_t CHARGE_DRAWLIST = 2;
    constexpr size_t CHARGE_BUF      = tuning::kChargeBuf;
    char          g_chargeText[CHARGE_BUF] = {};
    bool          g_chargeVisible = false;
    sead::Color4f g_chargeColor = {1.0f, 1.0f, 1.0f, 1.0f}; 

    void chargePosCb(lotuskit::TextWriterExt* /*writer*/, sead::Vector2f* pos) {
        pos->x = tuning::kChargeX;
        pos->y = tuning::kChargeY;
    }

    void refuseConfiguration(const char* why) {
        static bool warned = false;
        if (warned) { return; }
        warned = true;
        char buf[96];
        nn::util::SNPrintf(buf, sizeof(buf), "[overlay] presentation config refused (%s)", why);
        totk::ui::emitDiagnostic(buf);
    }

    void refusePrimitiveUniformBuffer(const char* why) {
        static bool warned = false;
        if (warned) { return; }
        warned = true;
        char buf[112];
        nn::util::SNPrintf(buf, sizeof(buf),
                           "[overlay] primitive uniform buffer refused (%s)", why);
        totk::ui::emitDiagnostic(buf);
    }

    void bannerPosCb(lotuskit::TextWriterExt* /*writer*/, sead::Vector2f* pos) {
        pos->x = g_bannerX;
        pos->y = g_bannerY;
    }

#if OVERLAY_DEBUG_HUD
    constexpr size_t HUD_DRAWLIST  = 1;
    constexpr size_t HUD_BUF       = tuning::kHudBuf;
    constexpr size_t HUD_MAX_LINES = tuning::kHudMaxLines;

    char          g_hudText[HUD_BUF] = {};
    sead::Color4f g_hudColor         = {1.0f, 1.0f, 1.0f, 1.0f}; 

    float g_hudLineY[HUD_MAX_LINES] = {};

    template <size_t I>
    void hudLineCb(lotuskit::TextWriterExt* /*writer*/, sead::Vector2f* pos) {
        pos->x = g_pres.hud.x;
        pos->y = g_hudLineY[I];
    }
    template <size_t... Is>
    constexpr std::array<lotuskit::TextWriterDrawCallback*, sizeof...(Is)>
    makeHudLineCbs(std::index_sequence<Is...>) { return { &hudLineCb<Is>... }; }
    constexpr auto g_hudLineCbs = makeHudLineCbs(std::make_index_sequence<HUD_MAX_LINES>{});
#endif
} 

bool configurePrimitiveUniformBuffer(std::uint32_t bytes) {
    if (g_presLocked) {
        refusePrimitiveUniformBuffer("after install/first frame");
        return false;
    }
    if (!totk::ui::isSanePrimitiveUniformBufferBytes(bytes)) {
        refusePrimitiveUniformBuffer("invalid size/alignment");
        return false;
    }
    lotuskit::DebugDrawHooks::setPrimitiveUniformBufferBytes(bytes);
    return true;
}

bool configurePresentation(const totk::ui::OverlayPresentation& presentation) {
    if (g_presLocked) {
        refuseConfiguration("after install/first frame");
        return false;
    }
    if (!totk::ui::isSanePresentation(presentation)) {
        refuseConfiguration("insane values");
        return false;
    }
    g_pres = presentation;          
#if OVERLAY_DEBUG_HUD
    g_hudColor = toSead(g_pres.hud.color);
#endif
    return true;
}

void showBanner(const char* line1, const char* line2, int ttlTicks, const sead::Color4f& color) {
    const size_t cap = g_pres.banner.bufferBytes < sizeof(g_bannerText)
                           ? g_pres.banner.bufferBytes : sizeof(g_bannerText);
    if (line2) {
        nn::util::SNPrintf(g_bannerText, cap, "%s\n%s", line1, line2);
    } else {
        nn::util::SNPrintf(g_bannerText, cap, "%s", line1);
    }

    const int maxChars = totk::ui::measureLongestDisplayLine(g_bannerText);
    g_bannerX = totk::ui::bannerLeftX(maxChars, g_pres.banner.glyphAdvance, g_pres.banner.scale,
                                      tuning::kScreenW, g_pres.banner.rightMargin);
    g_bannerY     = g_pres.banner.topY;
    g_bannerColor = color;
    g_bannerTtlTicks = ttlTicks;
}

void showBanner(const char* line1, const char* line2, int ttlTicks) {
    showBanner(line1, line2, ttlTicks, toSead(g_pres.banner.defaultColor));
}

void showChargeMeter(const char* friendlyName, unsigned steps, unsigned threshold, bool full) {
    totk::ui::buildChargeMeterText(g_chargeText, sizeof(g_chargeText),
                                   friendlyName, steps, threshold, full);
    g_chargeColor = sead::Color4f{1.0f, 1.0f, 1.0f, 1.0f};
    g_chargeVisible = true;
}

void hideChargeMeter() {
    g_chargeVisible = false;
    g_chargeText[0] = '\0';
}

void setTextVisible(bool visible) {
    lotuskit::DebugDrawHooks::setTextVisible(visible);
}

bool textVisible() {
    return lotuskit::DebugDrawHooks::textVisible();
}

void tick() {
    g_presLocked = true;  
    if (lotuskit::TextWriter::debugDrawerInternalHeap == nullptr) { return; } 
    if (!g_gfxInited) {
        lotuskit::TextWriter::createFrameHeap();   
        g_gfxInited = true;
        totk::ui::emitDiagnostic("[overlay] frame heap created (text drawing live)");
    }

    lotuskit::TextWriter::resetFrame();            

    if (!textVisible()) {
        if (g_bannerTtlTicks > 0) g_bannerTtlTicks--;
        return;
    }

    if (g_bannerTtlTicks > 0) {
        g_bannerTtlTicks--;
        lotuskit::TextWriter::appendNewDrawNode(
            BANNER_DRAWLIST, g_bannerText, bannerPosCb, g_pres.banner.scale, &g_bannerColor);
    }

    if (g_chargeVisible && g_chargeText[0] != '\0') {
        lotuskit::TextWriter::appendNewDrawNode(
            CHARGE_DRAWLIST, g_chargeText, chargePosCb, tuning::kChargeScale, &g_chargeColor);
    }

#if OVERLAY_DEBUG_HUD
    if (g_hudText[0] != '\0') {
        const char* p = g_hudText;
        char line[tuning::kHudLineBuf];
        const size_t maxLines = g_pres.hud.maxLines < HUD_MAX_LINES
                                    ? g_pres.hud.maxLines : HUD_MAX_LINES;
        size_t i = 0;
        while (*p != '\0' && i < maxLines) {
            totk::ui::hudNextLine(p, line, sizeof(line));
            lotuskit::TextWriter::appendNewDrawNode(HUD_DRAWLIST, line, g_hudLineCbs[i],
                                                    g_pres.hud.scale, &g_hudColor);
            i++;
        }
    }
#endif
}

#if OVERLAY_DEBUG_HUD
namespace hud {
    void begin() { g_hudText[0] = '\0'; }

    void linef(const char* fmt, ...) {
        char line[HUD_BUF];
        va_list ap;
        va_start(ap, fmt);
        nn::util::VSNPrintf(line, sizeof(line), fmt, ap);
        va_end(ap);
        const size_t cap = g_pres.hud.bufferBytes < HUD_BUF ? g_pres.hud.bufferBytes : HUD_BUF;
        totk::ui::hudAppendLine(g_hudText, cap, line);
    }

    void end() {
        int lines = totk::ui::countDisplayLines(g_hudText);
        const int maxLines = (int)(g_pres.hud.maxLines < HUD_MAX_LINES
                                       ? g_pres.hud.maxLines : HUD_MAX_LINES);
        if (lines > maxLines) { lines = maxLines; }
        totk::ui::hudLineYs(g_hudLineY, HUD_MAX_LINES, lines,
                            g_pres.hud.lineHeight, g_pres.hud.scale,
                            tuning::kScreenH, g_pres.hud.bottomMargin);
    }
} 
#endif

HOOK_DEFINE_INLINE(StealHeapHook) {
    static constexpr auto s_name = "engine::steal_heap";
    inline static sead::Heap* stolenHeap = nullptr;
    static void Callback(exl::hook::InlineCtx* ctx) {
        constexpr auto xi = TOTK_VERSION == 100 ? 19 : 22;
        stolenHeap = reinterpret_cast<sead::Heap*>(ctx->X[xi]);
        lotuskit::TextWriter::assignHeap(stolenHeap);

    }
};

void installHooks() {
    g_presLocked = true;  
    StealHeapHook::Install();
    lotuskit::DebugDrawHooks::BootupInitDebugDrawersHook::Install();
    lotuskit::DebugDrawHooks::DebugDrawLayerMaskHook::Install();
    lotuskit::DebugDrawHooks::DebugDrawHook::Install();
    totk::ui::emitDiagnostic("[overlay] hooks installed (steal-heap, agl-init, layer-mask, debug-draw)");
}

} 
