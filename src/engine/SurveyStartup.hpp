// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

namespace sead { class Heap; }

namespace zonai_survey::engine {

[[nodiscard]] bool startupImageSupported(std::uintptr_t mainBase);
void installAssetRedirect();
[[nodiscard]] bool prepareSurveyAssets(sead::Heap* heap);

}
