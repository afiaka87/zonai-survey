// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#include <lib.hpp>

#include <cmath>
#include <cstring>

#include "TerrainProber.hpp"
#include "PerfCounters.hpp"

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

void TerrainProber::arm(float linkX, float linkY, float linkZ, float headingX, float headingZ,
                        std::uint32_t sceneGeneration) {
    headingX_ = headingX;
    headingZ_ = headingZ;

    armed_.store(false, std::memory_order_release);
    authorised_.store(0, std::memory_order_release);
    completed_.store(0, std::memory_order_release);

    recovered_.store(0, std::memory_order_relaxed);
    rangeClipped_.store(0, std::memory_order_relaxed);
    raycasts_.store(0, std::memory_order_relaxed);
    sampleTicks_.store(0, std::memory_order_relaxed);

    originX_ = linkX;
    originY_ = linkY;
    originZ_ = linkZ;
    scene_ = sceneGeneration;
    for (std::uint32_t i = 0; i < pure::kProbeCount; ++i) samples_[i] = pure::TerrainSample{};

    for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke) carryY_[spoke] = linkY;

    armed_.store(true, std::memory_order_release);
}

void TerrainProber::disarm() {
    armed_.store(false, std::memory_order_release);
    authorised_.store(0, std::memory_order_release);
}

std::uint32_t TerrainProber::service(RaycastFn original, const void* liveQueryObject) {
    if (!original || !liveQueryObject) return 0;
    if (!armed_.load(std::memory_order_acquire)) return 0;

    const std::uint32_t authorised = authorised_.load(std::memory_order_acquire);
    std::uint32_t next = completed_.load(std::memory_order_relaxed);

    const std::uint32_t batch = pure::probeBatchSize(authorised, next);
    if (batch == 0) return 0;

    const std::uint64_t sampleStart = perf::now();

    std::memcpy(queryObject_, liveQueryObject, sizeof(queryObject_));

    std::uint32_t issued = 0;
    while (issued < batch) {
        samples_[next] = castNode(original, next);
        ++next;
        ++issued;
    }

    sampleTicks_.fetch_add(perf::now() - sampleStart, std::memory_order_relaxed);

    completed_.store(next, std::memory_order_release);
    return issued;
}

bool TerrainProber::castSegment(RaycastFn original, float fromX, float fromY, float fromZ,
                                float toX, float toY, float toZ, pure::TerrainSample& out) {

    const float from[3] = {fromX, fromY, fromZ};
    const float to[3] = {toX, toY, toZ};

    queryObject_[layout::kRaycastHit] = 0;
    raycasts_.fetch_add(1, std::memory_order_relaxed);
    original(from, to, queryObject_, nullptr, kTerrainMask, 0U);

    out = pure::TerrainSample{};
    if (queryObject_[layout::kRaycastHit] == 0) return false;

    out.x = readFloat(queryObject_, layout::kRaycastPosition + 0);
    out.y = readFloat(queryObject_, layout::kRaycastPosition + 4);
    out.z = readFloat(queryObject_, layout::kRaycastPosition + 8);
    out.normalX = readFloat(queryObject_, layout::kRaycastNormal + 0);
    out.normalY = readFloat(queryObject_, layout::kRaycastNormal + 4);
    out.normalZ = readFloat(queryObject_, layout::kRaycastNormal + 8);

    if (!__builtin_isfinite(out.x) || !__builtin_isfinite(out.y) ||
        !__builtin_isfinite(out.z) || !__builtin_isfinite(out.normalX) ||
        !__builtin_isfinite(out.normalY) || !__builtin_isfinite(out.normalZ)) {
        out = pure::TerrainSample{};
        return false;
    }
    out.hit = true;
    return true;
}

pure::TerrainSample TerrainProber::castNode(RaycastFn original, std::uint32_t index) {
    const std::uint32_t spoke = pure::probeSpoke(index);

    if (!pure::sampleWithinRange(pure::probeRing(index), spoke)) {
        rangeClipped_.fetch_add(1, std::memory_order_relaxed);
        return pure::TerrainSample{};
    }
    const pure::FanRay ray = pure::fanRayFor(index, originX_, originZ_, carryY_[spoke],
                                             headingX_, headingZ_);

    pure::TerrainSample node{};
    pure::FanRay used = ray;
    if (!castSegment(original, ray.fromX, ray.fromY, ray.fromZ, ray.toX, ray.toY, ray.toZ,
                     node)) {

        const pure::FanRay wide = pure::recoveryRayFor(
            index, originX_, originZ_, carryY_[spoke], headingX_, headingZ_);
        if (!castSegment(original, wide.fromX, wide.fromY, wide.fromZ, wide.toX, wide.toY,
                         wide.toZ, node)) {

            return pure::TerrainSample{};
        }
        used = wide;
        ++recovered_;
    }

    const float dx = node.x - used.fromX;
    const float dy = node.y - used.fromY;
    const float dz = node.z - used.fromZ;
    node.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!__builtin_isfinite(node.distance)) return pure::TerrainSample{};

    carryY_[spoke] = node.y;
    return node;
}

}
