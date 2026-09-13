// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstdint>

namespace agl {
class DrawContext;
}

namespace totk::render {

bool configureDepthState(std::uintptr_t mainBase);

bool applyDepthTestedOverlay(agl::DrawContext* drawCtx);

void restoreSceneState(agl::DrawContext* drawCtx);

}
