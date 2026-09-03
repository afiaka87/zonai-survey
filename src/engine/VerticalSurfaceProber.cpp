// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include <cmath>
#include <cstring>

#include "PerfCounters.hpp"
#include "VerticalSurfaceProber.hpp"

namespace zonai_survey::engine {
namespace {

namespace layout = totk::engine::layout;
constexpr std::uint32_t kTerrainMask = 0x20;

float readFloat(const unsigned char* base, std::ptrdiff_t offset) {
    float value = 0.0f;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

}  

void VerticalSurfaceProber::arm(float linkX, float linkY, float linkZ,
                                float headingX, float headingZ,
                                std::uint32_t sceneGeneration) {
    armed_.store(false, std::memory_order_release);
    completed_.store(0, std::memory_order_release);
    raycastsIssued_.store(0, std::memory_order_release);
    slopeSkipped_.store(0, std::memory_order_relaxed);
    seeThrough_.store(0, std::memory_order_relaxed);
    sampleTicks_.store(0, std::memory_order_relaxed);
    originX_ = linkX;
    originY_ = linkY;
    originZ_ = linkZ;
    headingX_ = headingX;
    headingZ_ = headingZ;
    scene_ = sceneGeneration;
    for (std::uint32_t i = 0; i < pure::kWallSamples; ++i) samples_[i] = {};
    for (std::uint32_t i = 0; i < pure::kWallProbes; ++i) {
        sampleCounts_[i] = 0;
        shallowSamples_[i] = {};
    }
    armed_.store(true, std::memory_order_release);
}

void VerticalSurfaceProber::disarm() {
    armed_.store(false, std::memory_order_release);
}

std::uint32_t VerticalSurfaceProber::service(RaycastFn original,
                                             const void* liveQueryObject) {
    if (!original || !liveQueryObject ||
        !armed_.load(std::memory_order_acquire))
        return 0;

    std::memcpy(queryObject_, liveQueryObject, sizeof(queryObject_));

    std::uint32_t next = completed_.load(std::memory_order_relaxed);
    if (next >= pure::kWallProbes) return 0;

    const std::uint32_t remaining = pure::kWallProbes - next;
    const std::uint32_t batch = remaining < pure::kWallProbesPerService
                                    ? remaining
                                    : pure::kWallProbesPerService;
    std::uint32_t raycasts = 0;
    const std::uint64_t sampleStart = perf::now();
    for (std::uint32_t issued = 0; issued < batch; ++issued, ++next)
        raycasts += castNode(original, next);
    sampleTicks_.fetch_add(perf::now() - sampleStart, std::memory_order_relaxed);

    raycastsIssued_.fetch_add(raycasts, std::memory_order_relaxed);
    if (next >= pure::kWallProbes)
        armed_.store(false, std::memory_order_release);
    completed_.store(next, std::memory_order_release);
    return batch;
}

VerticalSurfaceProber::SegmentCastResult VerticalSurfaceProber::castSegment(
    RaycastFn original, float fromX, float fromY, float fromZ, float toX,
    float toY, float toZ, pure::TerrainSample& out) {
    const float from[3] = {fromX, fromY, fromZ};
    const float to[3] = {toX, toY, toZ};

    queryObject_[layout::kRaycastHit] = 0;
    original(from, to, queryObject_, nullptr, kTerrainMask, 0U);

    out = {};
    if (queryObject_[layout::kRaycastHit] == 0) return SegmentCastResult::Clear;
    out.x = readFloat(queryObject_, layout::kRaycastPosition + 0);
    out.y = readFloat(queryObject_, layout::kRaycastPosition + 4);
    out.z = readFloat(queryObject_, layout::kRaycastPosition + 8);
    out.normalX = readFloat(queryObject_, layout::kRaycastNormal + 0);
    out.normalY = readFloat(queryObject_, layout::kRaycastNormal + 4);
    out.normalZ = readFloat(queryObject_, layout::kRaycastNormal + 8);
    if (!__builtin_isfinite(out.x) || !__builtin_isfinite(out.y) ||
        !__builtin_isfinite(out.z) || !__builtin_isfinite(out.normalX) ||
        !__builtin_isfinite(out.normalY) || !__builtin_isfinite(out.normalZ)) {
        out = {};
        return SegmentCastResult::Invalid;
    }
    out.hit = true;
    return SegmentCastResult::Hit;
}

std::uint32_t VerticalSurfaceProber::castNode(RaycastFn original,
                                              std::uint32_t index) {
    const pure::WallRay ray = pure::wallRayFor(index, originX_, originY_,
                                               originZ_, headingX_, headingZ_);
    pure::TerrainSample layers[pure::kWallDepthLayers]{};
    std::uint32_t stored = 0;
    pure::TerrainSample shallow{};
    const std::uint32_t raycasts =
        castProbeRay(original, ray, layers, stored, shallow);

    for (std::uint32_t layer = 0; layer < pure::kWallDepthLayers; ++layer)
        samples_[pure::wallSampleIndex(index, layer)] =
            layer < stored ? layers[layer] : pure::TerrainSample{};
    if (stored != 0 && shallow.hit && shallow.distance < layers[0].distance)
        seeThrough_.fetch_add(1, std::memory_order_relaxed);
    sampleCounts_[index] = static_cast<std::uint8_t>(stored);
    shallowSamples_[index] = shallow;
    return raycasts;
}

std::uint32_t VerticalSurfaceProber::castProbeRay(
    RaycastFn original, const pure::WallRay& ray,
    pure::TerrainSample* layersOut, std::uint32_t& storedOut,
    pure::TerrainSample& shallowOut) {
    storedOut = 0;
    shallowOut = {};
    const float dx = ray.toX - ray.fromX;
    const float dy = ray.toY - ray.fromY;
    const float dz = ray.toZ - ray.fromZ;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!__builtin_isfinite(length) || length < 1.0f) return 0;
    const float directionX = dx / length;
    const float directionY = dy / length;
    const float directionZ = dz / length;

    std::uint32_t raycasts = 0;
    float cursor = 0.0f;
    float lastStoredDistance = -pure::kWallMinLayerSeparationMeters;
    while (raycasts < pure::kWallMaxRaycastsPerProbe &&
           storedOut < pure::kWallDepthLayers &&
           cursor + pure::kWallRayAdvanceMeters < length) {
        const float fromX = ray.fromX + directionX * cursor;
        const float fromY = ray.fromY + directionY * cursor;
        const float fromZ = ray.fromZ + directionZ * cursor;
        pure::TerrainSample node{};
        ++raycasts;
        if (castSegment(original, fromX, fromY, fromZ, ray.toX, ray.toY,
                        ray.toZ, node) != SegmentCastResult::Hit)
            break;

        const float hitX = node.x - ray.fromX;
        const float hitY = node.y - ray.fromY;
        const float hitZ = node.z - ray.fromZ;
        const float distance = hitX * directionX + hitY * directionY +
                               hitZ * directionZ;
        if (!__builtin_isfinite(distance) || distance < cursor - 0.1f ||
            distance > length + 0.1f)
            break;
        node.distance = distance;
        node.viewCosine =
            pure::rayViewCosine(node.normalX, node.normalY, node.normalZ,
                                directionX, directionY, directionZ);

        const bool advancedFromCastOrigin =
            distance - cursor >= pure::kWallMinHitTravelMeters;
        const bool steepEnough = pure::wallNormalIsSteep(node.normalY);
        if (advancedFromCastOrigin && !steepEnough)
            slopeSkipped_.fetch_add(1, std::memory_order_relaxed);
        if (advancedFromCastOrigin && steepEnough &&
            distance - lastStoredDistance >=
                pure::kWallMinLayerSeparationMeters) {
            layersOut[storedOut] = node;
            lastStoredDistance = distance;
            ++storedOut;
        } else if (advancedFromCastOrigin && !steepEnough && !shallowOut.hit) {
            shallowOut = node;
        }

        const float afterHit = distance + pure::kWallRayAdvanceMeters;
        const float forcedProgress = cursor + pure::kWallRayAdvanceMeters;
        cursor = afterHit > forcedProgress ? afterHit : forcedProgress;
    }
    return raycasts;
}

}  
