// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <cstdint>

#include "totk/harness/Module.hpp"

namespace solo {
void init(std::uintptr_t mainBase, const wwpg::Module& module);
void tick(void* npadDevice);
}
