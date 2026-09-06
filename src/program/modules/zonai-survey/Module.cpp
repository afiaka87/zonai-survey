// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include "ZonaiSurveyModule.hpp"
#include "ZonaiSurveyIntegration.hpp"

#include "GlyphController.hpp"
#include "GlyphRenderer.hpp"
#include "PerfCounters.hpp"
#include "PerfReport.hpp"
#include "ScanBatching.hpp"
#include "ScanController.hpp"
#include "ScanPresentation.hpp"
#include "ScanRenderer.hpp"
#include "totk/engine/Npad.hpp"
#include "totk/ui/Overlay.hpp"

#ifndef SOLO_HARNESS_TEXT
#define SOLO_HARNESS_TEXT 1
#endif

namespace {

constexpr std::uint64_t BTN_ZL = 1ull << 8;
constexpr std::uint64_t BTN_UP = 1ull << 13;

zonai_survey::feature::ScanController g_scan{};
zonai_survey::feature::GlyphController g_glyphs{};
totk::engine::NpadReader g_npad{};
std::uint64_t g_previousButtons = 0;
std::uint64_t g_ownedSurveyButton = 0;
bool g_ready = false;
zonai_survey::feature::ScanState g_previousState = zonai_survey::feature::ScanState::Idle;

char g_reachText[64]{};
#if SOLO_HARNESS_TEXT
char g_objectsText[128]{};
char g_controlsText[192]{};
char g_requirementText[160]{};

void updateModulePresentation() {
    nn::util::SNPrintf(g_objectsText, sizeof(g_objectsText),
                       "survey none - it reads the world and spawns nothing");
    nn::util::SNPrintf(g_controlsText, sizeof(g_controlsText), "ZL+D-Up survey");
    nn::util::SNPrintf(g_requirementText, sizeof(g_requirementText),
                       "no save data and no world edits; the reading fades on its own");
}
#endif

void appendText(char*& out, const char* end, const char* text) {
    while (text && *text != '\0' && out < end) *out++ = *text++;
}

void appendUInt(char*& out, const char* end, std::uint32_t value) {
    char digits[12]{};
    int count = 0;
    if (value == 0) digits[count++] = '0';
    while (value > 0 && count < 12) {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (count > 0 && out < end) *out++ = digits[--count];
}

void buildReachText(const zonai_survey::feature::ScanDiagnostics& diagnostics) {
    char* out = g_reachText;
    const char* const end = g_reachText + sizeof(g_reachText) - 1;
    appendUInt(out, end, diagnostics.reachMeters);
    appendText(out, end, " m of ground (");
    appendUInt(out, end, diagnostics.reachRing);
    appendText(out, end, " of ");
    appendUInt(out, end, zonai_survey::pure::kRings);
    appendText(out, end, " rings)");
    *out = '\0';
}

void moduleInit(std::uintptr_t mainBase) {
    g_scan.initialize(mainBase);
    g_glyphs.initialize(mainBase);
    g_previousButtons = 0;
    g_ownedSurveyButton = 0;
    g_previousState = zonai_survey::feature::ScanState::Idle;
    g_reachText[0] = '\0';
#if SOLO_HARNESS_TEXT
    updateModulePresentation();
#endif
    g_ready = true;
    Logging.Log("[zonai-survey] module init");
}

void moduleEnter() {
    g_previousButtons = 0;
    g_ownedSurveyButton = 0;
    g_previousState = zonai_survey::feature::ScanState::Idle;
#if SOLO_HARNESS_TEXT
    updateModulePresentation();
#endif
}

void serviceSurveyInput(const totk::engine::NpadFrame& frame, std::uint64_t buttons,
                        std::uint64_t pressed) {
    if ((buttons & BTN_ZL) != 0) g_ownedSurveyButton |= buttons & BTN_UP;
    frame.maskOwnedButtons(g_ownedSurveyButton);

    if ((buttons & BTN_ZL) == 0 || (pressed & BTN_UP) == 0) return;
    const auto verdict = zonai_survey::integration::triggerSurvey();
    if (verdict != zonai_survey::pure::ScanVerdict::Accepted) {
        overlay::showBanner("Cannot survey here",
                            zonai_survey::presentation::displayText(verdict), 150);
        return;
    }
}

void serviceSurveyCompletion() {
    {
        zonai_survey::engine::perf::Timer scanTimer(zonai_survey::engine::perf::gameScan);
        g_scan.tick();
    }
    {
        zonai_survey::engine::perf::Timer glyphTimer(zonai_survey::engine::perf::gameGlyphs);
        g_glyphs.tick();
    }

    const auto state = g_scan.state();
    if (state == zonai_survey::feature::ScanState::Holding &&
        g_previousState == zonai_survey::feature::ScanState::Pulsing) {
        buildReachText(g_scan.diagnostics());
        overlay::showBanner("Survey complete", g_reachText, 180);

        const auto& glyphs = g_glyphs.diagnostics();
        const auto& cut = glyphs.filtered;
        Logging.Log(
            "[zonai-survey] glyphs published=%u map=%u roster=%u walked=%u hits=%u live_ok=%d "
            "farthest_live=%dm drawn=%u offscreen=%u",
            glyphs.published, glyphs.fromMap, glyphs.fromRoster, glyphs.rosterWalked,
            glyphs.rosterHits, glyphs.rosterAvailable ? 1 : 0,
            static_cast<int>(glyphs.farthestLiveMeters),
            zonai_survey::render::lastGlyphsDrawn(),
            zonai_survey::render::lastGlyphsOffscreen());
        Logging.Log(
            "[zonai-survey] glyph filters cone=%u cap=%u on_player=%u duplicate_of_live=%u "
            "names_collided=%u",
            cut.outsideCone, cut.lostToCap, cut.onPlayer, cut.overridden,
            zonai_survey::render::lastNamesDropped());
    }
    g_previousState = state;
}

#if OVERLAY_DEBUG_HUD
std::uint64_t g_perfWindowStart = 0;
std::uint32_t g_perfIdleWindows = 0;

void flushGamePerf() {
    namespace perf = zonai_survey::engine::perf;
    const std::uint64_t now = perf::now();
    const std::uint64_t freq = perf::frequency();
    if (g_perfWindowStart == 0) {
        g_perfWindowStart = now;
        return;
    }
    const std::uint64_t elapsed = now - g_perfWindowStart;
    if (elapsed < freq) return;

    const auto state = g_scan.state();
    const bool idle = state == zonai_survey::feature::ScanState::Idle;
    const bool report = !idle || (g_perfIdleWindows++ % 10u) == 0;
    if (report) {
        using zonai_survey::pure::perfMicros;
        using zonai_survey::pure::perfMillis;
        Logging.Log(
            "[zonai-survey] perf game %ums %s segs=%u tick=%uus/%u scan=%uus "
            "rebuild=%uus/%u glyphs=%uus status=%uus/%u changes=%u "
            "worker_est=%uus/%u",
            perfMillis(elapsed, freq),
            state == zonai_survey::feature::ScanState::Pulsing
                ? "pulse"
                : (state == zonai_survey::feature::ScanState::Holding ? "hold" : "idle"),
            g_scan.diagnostics().segments,
            perfMicros(perf::gameTick.sum, freq), perf::gameTick.count,
            perfMicros(perf::gameScan.sum, freq),
            perfMicros(perf::gameRebuild.sum, freq), perf::gameRebuild.count,
            perfMicros(perf::gameGlyphs.sum, freq),
            perfMicros(perf::gameStatus.sum, freq), perf::gameStatus.count,
            perf::gameStatusChanges,
            perfMicros(perf::workerSum.exchange(0, std::memory_order_relaxed) * 16u, freq),
            perf::workerSeen.exchange(0, std::memory_order_relaxed));
        perf::workerCalls.store(0, std::memory_order_relaxed);
    } else {
        perf::workerSum.store(0, std::memory_order_relaxed);
        perf::workerCalls.store(0, std::memory_order_relaxed);
        perf::workerSeen.store(0, std::memory_order_relaxed);
    }
    if (!idle) g_perfIdleWindows = 0;
    perf::gameTick.reset();
    perf::gameScan.reset();
    perf::gameRebuild.reset();
    perf::gameGlyphs.reset();
    perf::gameStatus.reset();
    perf::gameStatusChanges = 0;
    g_perfWindowStart = now;
}
#else
void flushGamePerf() {}
#endif

void moduleTick(void* npadDevice) {
    if (npadDevice) overlay::tick();
    if (!g_ready) return;
    flushGamePerf();
    zonai_survey::engine::perf::Timer tickTimer(zonai_survey::engine::perf::gameTick);

    if (npadDevice) {
        const auto frame = g_npad.read(npadDevice);
        const std::uint64_t buttons = frame.snapshot().buttons;
        const std::uint64_t pressed = buttons & ~g_previousButtons;
        g_previousButtons = buttons;
        g_ownedSurveyButton &= buttons;
        serviceSurveyInput(frame, buttons, pressed);
    }
    serviceSurveyCompletion();
}

bool moduleRequestExit() { return true; }

#if SOLO_HARNESS_TEXT
const char* moduleStatusText() {
    const auto scanState = g_scan.state();
    if (scanState != zonai_survey::feature::ScanState::Idle) {
        return scanState == zonai_survey::feature::ScanState::Pulsing ? "Survey pulse active"
                                                                     : "Survey reading held";
    }
    return "ready";
}

std::uint32_t g_statusHash = 0;

const char* moduleStatus() {
    zonai_survey::engine::perf::Timer statusTimer(zonai_survey::engine::perf::gameStatus);
    const char* const text = moduleStatusText();
    std::uint32_t hash = 2166136261u;
    for (const char* p = text; *p; ++p)
        hash = (hash ^ static_cast<std::uint8_t>(*p)) * 16777619u;
    if (hash != g_statusHash) {
        g_statusHash = hash;
        ++zonai_survey::engine::perf::gameStatusChanges;
    }
    return text;
}
#endif

void moduleOnRaycast(wwpg::RaycastFn original, const void*, const void*, const void* object,
                     const void*, std::uint32_t, std::uint32_t) {
    namespace perf = zonai_survey::engine::perf;
    const bool sampled = (perf::workerSeen.fetch_add(1, std::memory_order_relaxed) & 15u) == 0;
    const std::uint64_t probeStart = sampled ? perf::now() : 0;
    g_scan.serviceProbes(reinterpret_cast<zonai_survey::engine::RaycastFn>(original), object);
    if (sampled) perf::addWorkerSample(perf::now() - probeStart);
}

constexpr wwpg::Module kModule{
    .name = "Zonai Survey",
#if SOLO_HARNESS_TEXT
    .actors = g_objectsText,
    .controls = g_controlsText,
    .requirement = g_requirementText,
#else
    .actors = nullptr,
    .controls = nullptr,
    .requirement = nullptr,
#endif
    .init = &moduleInit,
    .enter = &moduleEnter,
    .tick = &moduleTick,
    .requestExit = &moduleRequestExit,
#if SOLO_HARNESS_TEXT
    .status = &moduleStatus,
#else
    .status = nullptr,
#endif
    .onRaycast = &moduleOnRaycast,
    .aim = nullptr,
};
static_assert(kModule.init && kModule.tick && kModule.onRaycast);

}  

namespace zonai_survey::integration {

pure::ScanVerdict triggerSurvey() {
    const pure::ScanVerdict verdict = g_scan.trigger();
    if (verdict != pure::ScanVerdict::Accepted) return verdict;

    g_glyphs.onPulse(g_scan.originX(), g_scan.originY(), g_scan.originZ(),
                     g_scan.headingX(), g_scan.headingZ(), g_scan.sceneGeneration());
    return verdict;
}

}  

namespace wwpg::modules {
const Module& zonaiSurvey() { return kModule; }
}  
