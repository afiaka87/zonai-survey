// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include <cstdint>
#include <cstring>

#include <nn/util.h>  

#include "totk/render/DepthState.hpp"

#include "totk/render/DrawOffsets.hpp"
#include "totk/ui/Diagnostics.hpp"

namespace totk::render {
namespace {

using ConstructFn = void (*)(void*);
using ApplyFn = void (*)(void*, agl::DrawContext*);

ConstructFn g_construct = nullptr;
ApplyFn g_apply = nullptr;
bool g_ready = false;
bool g_loggedApplyRefusal = false;

alignas(8) std::uint8_t g_drawState[kGraphicsContextSize]{};
alignas(8) std::uint8_t g_sceneRestore[kGraphicsContextSize]{};

void setDepth(std::uint8_t* state, bool test, bool write, std::uint8_t compare) {
    state[kGcDepthTestEnable] = test ? 1 : 0;
    state[kGcDepthWriteEnable] = write ? 1 : 0;
    state[kGcDepthFunc] = compare;
}

template <typename... Args>
void note(const char* format, Args... args) {
    char line[192];
    nn::util::SNPrintf(line, sizeof(line), format, args...);
    totk::ui::emitDiagnostic(line);
}

}  

bool configureDepthState(std::uintptr_t mainBase) {
    if (g_ready) return true;
    if (!mainBase) return false;

    g_construct = reinterpret_cast<ConstructFn>(mainBase + DrawOffsets::kGraphicsContextCtor.value);
    g_apply = reinterpret_cast<ApplyFn>(mainBase + DrawOffsets::kGraphicsContextApply.value);
    std::memset(g_drawState, 0, sizeof(g_drawState));
    std::memset(g_sceneRestore, 0, sizeof(g_sceneRestore));
    g_construct(g_drawState);
    g_construct(g_sceneRestore);

    setDepth(g_drawState, true, false, kGcDepthFuncNearerOrEqual);
    note("[totk-render] depth state ready; engine default test=%u write=%u cmp=%u, "
         "seam test=1 write=0 cmp=%u",
         g_sceneRestore[kGcDepthTestEnable], g_sceneRestore[kGcDepthWriteEnable],
         g_sceneRestore[kGcDepthFunc],
         static_cast<unsigned>(kGcDepthFuncNearerOrEqual));
    g_ready = true;
    return true;
}

bool applyDepthTestedOverlay(agl::DrawContext* drawCtx) {
    if (!g_ready || !drawCtx) {
        if (!g_loggedApplyRefusal) {
            g_loggedApplyRefusal = true;
            note("[totk-render] depth-correct draw refused: state ready=%d context=%p",
                 g_ready ? 1 : 0, static_cast<void*>(drawCtx));
        }
        return false;
    }
    g_apply(g_drawState, drawCtx);
    return true;
}

void restoreSceneState(agl::DrawContext* drawCtx) {
    if (g_ready && drawCtx) g_apply(g_sceneRestore, drawCtx);
}

}  
