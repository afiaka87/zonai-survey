// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include "Audio.hpp"
#include "GlyphRenderer.hpp"
#include "ModVersion.hpp"
#include "ScanBatching.hpp"
#include "ScanRenderer.hpp"
#include "modules/zonai-survey/ZonaiSurveyModule.hpp"
#include "totk/engine/Totk121Offsets.hpp"
#include "totk/harness/SoloHarness.hpp"
#include "totk/ui/Overlay.hpp"

namespace {
using totk::engine::Totk121Offsets;

HOOK_DEFINE_TRAMPOLINE(RayCastWorkerHook) {
    static u64 OriginalThunk(const void* from, const void* to, const void* object,
                             const void* out, u32 mask, u32 flag) {
        return Orig(from, to, object, out, mask, flag);
    }

    static u64 Callback(const void* from, const void* to, const void* object, const void* out,
                        u32 mask, u32 flag) {
        const u64 result = Orig(from, to, object, out, mask, flag);
        const auto& module = wwpg::modules::zonaiSurvey();
        if (module.onRaycast) {
            module.onRaycast(&OriginalThunk, from, to, object, out, mask, flag);
        }
        return result;
    }
};

HOOK_DEFINE_TRAMPOLINE(NpadCalcHook) {
    static void Callback(void* device) {
        Orig(device);
        solo::tick(device);
    }
};

}  

extern "C" void exl_main(void*, void*) {
    exl::hook::Initialize();
    const uintptr_t mainBase = exl::util::modules::GetTargetStart();

    const bool ringRaised =
        overlay::configurePrimitiveUniformBuffer(zonai_survey::pure::kSurveyRingBytes);

    overlay::installHooks();
    zonai_survey::render::install(mainBase);
    lotuskit::DebugDrawHooks::setWorldDrawCallback(zonai_survey::render::drawGlyphs);

    solo::init(mainBase, wwpg::modules::zonaiSurvey());
    RayCastWorkerHook::InstallAtOffset(Totk121Offsets::kRaycastWorker.value);
    NpadCalcHook::InstallAtOffset(Totk121Offsets::kNpadCalc.value);
    audio::installHooks(mainBase);

    Logging.Log("[zonai-survey] %s depth-correct survey ready DRAW_RING=%u(%d)",
                zonai_survey::kModVersion, zonai_survey::pure::kSurveyRingBytes,
                ringRaised ? 1 : 0);
}

extern "C" NORETURN void exl_exception_entry() { EXL_ABORT("unreachable"); }
