// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

namespace zonai_survey::engine {

struct WaterSurfaceSample {
    bool valid = false;
    bool hasWater = false;
    float surfaceY = 0.0f;
    float bottomY = 0.0f;
};

class WaterSurfaceProbe {
  public:
    void begin(std::uintptr_t mainBase);
    void resetScene();
    [[nodiscard]] WaterSurfaceSample sample(float x, float z);

  private:
    [[nodiscard]] bool resolveScene();

    std::uintptr_t mainBase_ = 0;
    void* scene_ = nullptr;
};

}  
