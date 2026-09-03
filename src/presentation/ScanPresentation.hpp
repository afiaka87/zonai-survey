// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include "ScanVerdicts.hpp"

namespace zonai_survey::presentation {

inline const char* displayText(pure::ScanVerdict verdict) {
    switch (verdict) {
        case pure::ScanVerdict::Accepted: return "surveying";
        case pure::ScanVerdict::AlreadyPulsing:
            return "a scan is already travelling";
        case pure::ScanVerdict::PlayerUnresolved:
            return "cannot find Link right now";
    }
    return "not ready";
}

inline const char* displayText(pure::ScanAbandonReason reason) {
    switch (reason) {
        case pure::ScanAbandonReason::PlayerLost: return "lost Link mid-scan";
        case pure::ScanAbandonReason::WorldReloaded: return "the world reloaded";
        case pure::ScanAbandonReason::Expired: return "scan faded out";
    }
    return "unspecified";
}

}  
