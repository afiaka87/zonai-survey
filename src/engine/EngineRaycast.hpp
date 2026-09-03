// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

namespace zonai_survey::engine {

// Valid only inside the physics-worker callback with its live query object.
using RaycastFn = std::uint64_t (*)(const void*, const void*, const void*,
                                    const void*, std::uint32_t, std::uint32_t);

}  
