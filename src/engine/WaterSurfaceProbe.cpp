// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include "WaterSurfaceProbe.hpp"

#include "totk/engine/Pointer.hpp"

namespace zonai_survey::engine {
namespace {

constexpr std::ptrdiff_t kTerrainModuleIndirect = 0x0462E178;
constexpr std::ptrdiff_t kSearchTerrainSceneByName = 0x01334BAC;
constexpr std::ptrdiff_t kGetWaterDepth = 0x00CE8C94;

[[nodiscard]] bool finiteHeight(float value) {
    return value == value && value > -10000.0f && value < 10000.0f;
}

}  

void WaterSurfaceProbe::begin(std::uintptr_t mainBase) {
    mainBase_ = mainBase;
    scene_ = nullptr;
}

void WaterSurfaceProbe::resetScene() {
    scene_ = nullptr;
}

bool WaterSurfaceProbe::resolveScene() {
    if (totk::engine::isPlausibleAddress(
            reinterpret_cast<std::uintptr_t>(scene_))) return true;
    scene_ = nullptr;
    if (!totk::engine::isPlausibleAddress(mainBase_)) return false;
    const auto holder = totk::engine::readMemory<std::uintptr_t>(
        mainBase_ + kTerrainModuleIndirect);
    if (!totk::engine::isPlausibleAddress(holder)) return false;
    const auto module = totk::engine::readMemory<std::uintptr_t>(holder);
    if (!totk::engine::isPlausibleAddress(module)) return false;
    const char* sceneName = "MainField";
    const auto searchScene = reinterpret_cast<void* (*)(void*, const char**)>(
        mainBase_ + kSearchTerrainSceneByName);
    scene_ = searchScene(reinterpret_cast<void*>(module), &sceneName);
    return totk::engine::isPlausibleAddress(
        reinterpret_cast<std::uintptr_t>(scene_));
}

WaterSurfaceSample WaterSurfaceProbe::sample(float x, float z) {
    WaterSurfaceSample sample{};
    if (!resolveScene()) return sample;
    float heights[2] = {0.0f, 0.0f};
    const float position[2] = {x, z};
    const auto getWaterDepth =
        reinterpret_cast<std::int32_t (*)(float*, const float*, void*,
                                          std::int32_t, std::uint32_t)>(
            mainBase_ + kGetWaterDepth);
    const auto code = getWaterDepth(heights, position, scene_, -1, 0);
    sample.valid = true;
    sample.hasWater = code == 1 || code == 2;
    if (!sample.hasWater) return sample;
    if (!finiteHeight(heights[0]) || !finiteHeight(heights[1]) ||
        heights[0] < heights[1]) {
        sample.valid = false;
        sample.hasWater = false;
        return sample;
    }
    sample.surfaceY = heights[0];
    sample.bottomY = heights[1];
    return sample;
}

}  
