// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstdint>

#ifndef AUDIO_DEBUG_CAPTURE
#define AUDIO_DEBUG_CAPTURE 0
#endif

namespace audio {

    void installHooks(uintptr_t mainBase);

    bool playCue(const char* cueName);

    bool bankLocked();

    void primeBank();

#if AUDIO_DEBUG_CAPTURE

    namespace debug {
        unsigned emitTotal();
        unsigned uniqueCueCount();
        unsigned bankLockCount();
    }
#endif

}
