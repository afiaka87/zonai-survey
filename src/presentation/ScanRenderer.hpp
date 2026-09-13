// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstdint>

#include "ScanBatching.hpp"
#include "ScanReduction.hpp"
#include "SurveySweep.hpp"
#include "totk/render/PrimitiveGeometry.hpp"

namespace zonai_survey::render {

struct ScanFrame {
    std::uint32_t count = 0;
    std::uint32_t tick = 0;
    std::uint32_t scene = 0;

    std::uint32_t generation = 0;
    std::uint32_t groupCount = 0;
    pure::GroupSpan groups[pure::kMaxSurveyGroups]{};
    pure::ScanSegment segments[pure::kMaxDrawnSegments]{};

    totk::render::BeamStrip beams[pure::kMaxDrawnSegments]{};
};

bool install(std::uintptr_t mainBase);

void publish(const pure::ScanSegment* segments, std::uint32_t count, std::uint32_t tick,
             std::uint32_t sceneGeneration);

void publishTick(std::uint32_t tick);
void clear();

bool lastSurveyBatched();

std::uint32_t lastRefusedGroups();
std::uint32_t lastSurveyLines();

std::uint32_t lastDrawCalls();
std::uint32_t lastDroppedSegments();

}
