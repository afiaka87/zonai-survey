// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis; StealHeapHook derived from totk-lotuskit by aquacluck and contributors.

#pragma once

#include "totk/ui/TextWriter.hpp"  
#include "totk/ui/OverlayLayout.hpp"
#include <gfx/seadColor.h>

#ifndef OVERLAY_DEBUG_HUD
#define OVERLAY_DEBUG_HUD 0
#endif

namespace overlay {

    bool configurePrimitiveUniformBuffer(std::uint32_t bytes);

    bool configurePresentation(const totk::ui::OverlayPresentation& presentation);

    void installHooks();

    void tick();

    void setTextVisible(bool visible);
    bool textVisible();

    void showBanner(const char* line1, const char* line2, int ttlTicks, const sead::Color4f& color);

    void showBanner(const char* line1, const char* line2, int ttlTicks);

    void showChargeMeter(const char* friendlyName, unsigned steps, unsigned threshold, bool full);
    void hideChargeMeter();

#if OVERLAY_DEBUG_HUD
    namespace hud {
        void begin();
        void linef(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
        void end();
    }
#endif

} 
