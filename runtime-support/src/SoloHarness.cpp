// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis

#include "totk/harness/SoloHarness.hpp"

#ifndef SOLO_HARNESS_TEXT
#define SOLO_HARNESS_TEXT 1
#endif

#if SOLO_HARNESS_TEXT
#include "totk/harness/StatusText.hpp"
#include "totk/ui/Overlay.hpp"
#endif

namespace solo {
namespace {
const wwpg::Module* g_module = nullptr;

#if SOLO_HARNESS_TEXT
constexpr int kBannerTicks = 1500;
std::uint64_t g_tick = 0;
char g_lastStatus[totk::harness::kStatusCap]{};

void showInstructions() {
    char body[384]{};
    nn::util::SNPrintf(body, sizeof(body), "Controls: %s\nGoal: %s",
                       g_module->controls, g_module->requirement);
    overlay::showBanner(g_module->name, body, kBannerTicks,
                        sead::Color4f{1.0f, 1.0f, 1.0f, 1.0f});
}

#if OVERLAY_DEBUG_HUD
void publishHud() {
    overlay::hud::begin();
    overlay::hud::linef("%s", g_module->name);
    overlay::hud::linef("Objects: %s", g_module->actors);
    overlay::hud::linef("Controls: %s", g_module->controls);
    overlay::hud::linef("State: %s", g_module->status());
    if (g_module->aim) overlay::hud::linef("%s", g_module->aim());
    overlay::hud::linef("NEXT: %s", g_module->requirement);
    overlay::hud::end();
}
#endif
#endif 
}

void init(std::uintptr_t mainBase, const wwpg::Module& module) {
    g_module = &module;
    g_module->init(mainBase);
    g_module->enter();
#if SOLO_HARNESS_TEXT
    totk::harness::copyStatus(g_lastStatus, g_module->status());
    showInstructions();
#if OVERLAY_DEBUG_HUD
    publishHud();
#endif
#endif
}

void tick(void* npadDevice) {
#if SOLO_HARNESS_TEXT
    overlay::tick();
    ++g_tick;
#endif
    g_module->tick(npadDevice);
#if SOLO_HARNESS_TEXT
    const char* status = g_module->status();
    if (!totk::harness::statusEquals(status, g_lastStatus)) {
        totk::harness::copyStatus(g_lastStatus, status);
        overlay::showBanner(g_module->name, status, kBannerTicks,
                            sead::Color4f{0.75f, 1.0f, 0.80f, 1.0f});
#if OVERLAY_DEBUG_HUD
        publishHud();
#endif
    } else if ((g_tick % 15) == 0) {
#if OVERLAY_DEBUG_HUD
        publishHud();
#endif
    }
#endif 
}
}
