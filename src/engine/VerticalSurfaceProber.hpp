// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include <atomic>
#include <cstdint>

#include "TerrainProber.hpp"
#include "WallReduction.hpp"
#include "WallSurvey.hpp"

namespace zonai_survey::engine {

class VerticalSurfaceProber {
  public:
    void arm(float linkX, float linkY, float linkZ, float headingX,
             float headingZ, std::uint32_t sceneGeneration);
    void disarm();

    [[nodiscard]] std::uint32_t completed() const {
        return completed_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool complete() const {
        return completed() >= pure::kWallProbes;
    }
    [[nodiscard]] std::uint32_t sampleCount(std::uint32_t probe) const {
        return sampleCounts_[probe < pure::kWallProbes ? probe : 0];
    }
    [[nodiscard]] const pure::TerrainSample* samples(std::uint32_t probe) const {
        const std::uint32_t safe = probe < pure::kWallProbes ? probe : 0;
        return &samples_[pure::wallSampleIndex(safe, 0)];
    }
    [[nodiscard]] const pure::TerrainSample& sample(
        std::uint32_t probe, std::uint32_t layer = 0) const {
        const std::uint32_t safeProbe = probe < pure::kWallProbes ? probe : 0;
        const std::uint32_t safeLayer =
            layer < pure::kWallDepthLayers ? layer : 0;
        return samples_[pure::wallSampleIndex(safeProbe, safeLayer)];
    }

    [[nodiscard]] const pure::TerrainSample& shallowSample(
        std::uint32_t probe) const {
        return shallowSamples_[probe < pure::kWallProbes ? probe : 0];
    }
    [[nodiscard]] std::uint32_t raycastsIssued() const {
        return raycastsIssued_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint32_t slopeSkipped() const {
        return slopeSkipped_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint32_t seeThrough() const {
        return seeThrough_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t sampleTicks() const {
        return sampleTicks_.load(std::memory_order_relaxed);
    }

    std::uint32_t service(RaycastFn original, const void* liveQueryObject);

    void judgeBuried(std::uint32_t surfacedOut[pure::kWallDepthLayers],
                     std::uint32_t buriedOut[pure::kWallDepthLayers]) {
        pure::judgeBuriedSamples(samples_, sampleCounts_, surfacedOut,
                                 buriedOut);
    }

  private:
    enum class SegmentCastResult : std::uint8_t {
        Clear = 0,
        Hit,
        Invalid,
    };

    std::uint32_t castNode(RaycastFn original, std::uint32_t index);

    std::uint32_t castProbeRay(RaycastFn original, const pure::WallRay& ray,
                               pure::TerrainSample* layersOut,
                               std::uint32_t& storedOut,
                               pure::TerrainSample& shallowOut);
    SegmentCastResult castSegment(RaycastFn original, float fromX, float fromY,
                                  float fromZ, float toX, float toY, float toZ,
                                  pure::TerrainSample& out);

    std::atomic<bool> armed_{false};
    std::atomic<std::uint32_t> completed_{0};
    std::atomic<std::uint32_t> raycastsIssued_{0};
    std::atomic<std::uint32_t> slopeSkipped_{0};
    std::atomic<std::uint32_t> seeThrough_{0};
    std::atomic<std::uint64_t> sampleTicks_{0};
    float originX_ = 0.0f;
    float originY_ = 0.0f;
    float originZ_ = 0.0f;
    float headingX_ = 0.0f;
    float headingZ_ = 1.0f;
    std::uint32_t scene_ = 0;
    pure::TerrainSample samples_[pure::kWallSamples]{};
    std::uint8_t sampleCounts_[pure::kWallProbes]{};
    pure::TerrainSample shallowSamples_[pure::kWallProbes]{};
    alignas(16) unsigned char
        queryObject_[totk::engine::layout::kRaycastObjectSize]{};
};

}
