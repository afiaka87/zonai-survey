// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstring>

#include <lib.hpp>

namespace totk::ui {

using DiagnosticsSink = void (*)(const char* message);

inline DiagnosticsSink g_diagnosticsSink = nullptr;

inline void setDiagnosticsSink(DiagnosticsSink sink) { g_diagnosticsSink = sink; }

inline void emitDiagnostic(const char* message) {
    if (g_diagnosticsSink != nullptr) {
        g_diagnosticsSink(message);
        return;
    }
    svcOutputDebugString(message, strlen(message));
}

}
