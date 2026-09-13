// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <atomic>
#include <cstdint>

#include "EngineRaycast.hpp"
#include "PulseLattice.hpp"
#include "ScanReduction.hpp"
#include "totk/engine/Totk121Offsets.hpp"

namespace zonai_survey::engine {

class TerrainProber {
  public:

    void arm(float linkX, float linkY, float linkZ, float headingX, float headingZ,
             std::uint32_t sceneGeneration);

    void disarm();

    bool armed() const { return armed_.load(std::memory_order_acquire); }
    std::uint32_t sceneGeneration() const { return scene_; }

    void authoriseTo(std::uint32_t probeIndexLimit) {
        authorised_.store(probeIndexLimit, std::memory_order_release);
    }

    std::uint32_t completed() const { return completed_.load(std::memory_order_acquire); }

    std::uint32_t raycasts() const { return raycasts_.load(std::memory_order_relaxed); }

    std::uint64_t sampleTicks() const { return sampleTicks_.load(std::memory_order_relaxed); }

    std::uint32_t recovered() const { return recovered_.load(std::memory_order_relaxed); }

    std::uint32_t rangeClipped() const {
        return rangeClipped_.load(std::memory_order_relaxed);
    }

    const pure::TerrainSample& sample(std::uint32_t index) const {
        return samples_[index < pure::kProbeCount ? index : 0];
    }

    std::uint32_t service(RaycastFn original, const void* liveQueryObject);

  private:

    pure::TerrainSample castNode(RaycastFn original, std::uint32_t index);

    bool castSegment(RaycastFn original, float fromX, float fromY, float fromZ, float toX,
                     float toY, float toZ, pure::TerrainSample& out);

    std::atomic<bool> armed_{false};

    std::atomic<std::uint32_t> recovered_{0};
    std::atomic<std::uint32_t> rangeClipped_{0};

    std::atomic<std::uint32_t> raycasts_{0};
    std::atomic<std::uint64_t> sampleTicks_{0};
    std::atomic<std::uint32_t> authorised_{0};
    std::atomic<std::uint32_t> completed_{0};

    float originX_ = 0.0f;
    float originY_ = 0.0f;
    float originZ_ = 0.0f;
    float headingX_ = 0.0f;
    float headingZ_ = 1.0f;
    std::uint32_t scene_ = 0;

    pure::TerrainSample samples_[pure::kProbeCount]{};

    float carryY_[pure::kSpokes]{};

    alignas(16) unsigned char queryObject_[totk::engine::layout::kRaycastObjectSize]{};
};

}
