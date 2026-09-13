// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstddef>

namespace totk::harness {

inline constexpr std::size_t kStatusCap = 96;

inline bool statusEquals(const char* a, const char* b) {
    if (!a || !b) return a == b;
    for (int i = 0; i < 95; ++i) {
        if (a[i] != b[i]) return false;
        if (!a[i]) return true;
    }
    return true;
}

inline void copyStatus(char* dst, const char* text) {
    int i = 0;
    if (text) {
        for (; i < 95 && text[i]; ++i) dst[i] = text[i];
    }
    dst[i] = 0;
}

}
