// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis
#include <lib.hpp>

#include "ScanController.hpp"

#include <cstddef>

#include <nn/util.h>

#include "Audio.hpp"
#include "CameraHeading.hpp"
#include "PerfCounters.hpp"
#include "ScanRenderer.hpp"
#include "totk/engine/ActorRoster.hpp"
#include "totk/engine/Scene.hpp"
#include "totk/engine/Transform.hpp"

namespace zonai_survey::feature {
namespace {

namespace te = totk::engine;

constexpr std::uint32_t probesThroughRing(std::uint32_t ring) {
    return (ring + 1) * pure::kSpokes;
}

constexpr float kMaxWaterLiftMeters = 80.0f;

}

void ScanController::initialize(std::uintptr_t mainBase) {
    mainBase_ = mainBase;
    state_ = ScanState::Idle;
    diagnostics_ = ScanDiagnostics{};
    waterProbe_.begin(mainBase);
}

bool ScanController::resolveLink(float& x, float& y, float& z,
                                 std::uint32_t& sceneGeneration) {
    if (!mainBase_) return false;

    const auto scene = te::resolveScene(mainBase_);
    if (!scene.succeeded || !scene.value.isReady()) return false;

    const auto player = te::findResidentActor(scene.value, "Player");
    if (!player.succeeded) return false;

    const te::TransformService transforms{
        te::TransformFunctions::fromMainBase(mainBase_)};
    const auto pose = transforms.read(player.value, scene.value.token);
    if (!pose.succeeded) return false;

    x = pose.value.position.x;
    y = pose.value.position.y;
    z = pose.value.position.z;
    sceneGeneration = static_cast<std::uint32_t>(scene.value.token.value);
    return true;
}

pure::ScanVerdict ScanController::trigger() {
    if (state_ == ScanState::Pulsing) {
        diagnostics_.lastVerdict = pure::ScanVerdict::AlreadyPulsing;
        return diagnostics_.lastVerdict;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint32_t generation = 0;
    if (!resolveLink(x, y, z, generation)) {
        diagnostics_.lastVerdict = pure::ScanVerdict::PlayerUnresolved;
        return diagnostics_.lastVerdict;
    }

    originX_ = x;
    originY_ = y;
    originZ_ = z;
    scene_ = generation;
    tick_ = 0;
    segmentCount_ = 0;
    groundSegmentCount_ = 0;
    wallSegmentCount_ = 0;
    reducedThrough_ = 0;
    resetWallTracking();
    resetGroundTracking();
    wallIdentityJudged_ = false;
    for (auto& surfaced : wallSurfaced_) surfaced = 0;
    for (auto& buried : wallBuried_) buried = 0;
    spikesDropped_ = 0;
    hits_ = 0;
    reachRing_ = 0;
    groundHealed_ = 0;
    waterLifted_ = 0;
    crestArcs_ = 0;
    for (auto& sample : previousRing_) sample = pure::TerrainSample{};
    for (auto& sample : ringTwoBack_) sample = pure::TerrainSample{};
    for (auto& drawn : previousArcDrawn_) drawn = false;
    for (auto& drawn : currentArcDrawn_) drawn = false;

    waterProbe_.resetScene();

    const std::uint32_t pulses = diagnostics_.pulses;
    const pure::ScanAbandonReason lastEnding = diagnostics_.lastEnding;
    diagnostics_ = ScanDiagnostics{};
    diagnostics_.pulses = pulses + 1;
    diagnostics_.lastEnding = lastEnding;
    diagnostics_.lastVerdict = pure::ScanVerdict::Accepted;

    engine::cameraForward(headingX_, headingZ_);

    prober_.arm(x, y, z, headingX_, headingZ_, generation);
    wallProber_.arm(x, y, z, headingX_, headingZ_, generation);
    state_ = ScanState::Pulsing;

    audio::playCue(pure::kSurveyStartCueName);
    render::clear();
    Logging.Log(
        "[zonai-survey] scan armed at %d,%d,%d gen=%u probes=%u wall_probes=%u "
        "sound='%s' bank=%d",
        static_cast<int>(x), static_cast<int>(y), static_cast<int>(z),
        generation, pure::kProbeCount, pure::kWallProbes,
        pure::kSurveyStartCueName, audio::bankLocked() ? 1 : 0);
    return pure::ScanVerdict::Accepted;
}

void ScanController::abandon(pure::ScanAbandonReason reason) {
    if (state_ == ScanState::Idle) return;
    prober_.disarm();
    wallProber_.disarm();
    render::clear();
    state_ = ScanState::Idle;
    segmentCount_ = 0;
    groundSegmentCount_ = 0;
    wallSegmentCount_ = 0;
    reducedThrough_ = 0;
    resetWallTracking();
    resetGroundTracking();
    diagnostics_.lastEnding = reason;

    if (diagnostics_.wallProbesComplete < pure::kWallProbes)
        Logging.Log(
            "[zonai-survey] wall fan incomplete at teardown probes=%u/%u",
            diagnostics_.wallProbesComplete, pure::kWallProbes);
    Logging.Log("[zonai-survey] scan %s reason=%u",
                pure::isOrdinaryEnding(reason) ? "ended" : "ABANDONED",
                static_cast<unsigned>(reason));
}

void ScanController::rebuildSegments() {
    const std::uint32_t complete = prober_.completed();
    diagnostics_.probesComplete = complete;

    if (wallProber_.complete() && !wallIdentityJudged_) {
        wallProber_.judgeBuried(wallSurfaced_, wallBuried_);
        wallIdentityJudged_ = true;
        resetWallTracking();
        wallSegmentCount_ = 0;
    }

    while (reducedThrough_ < pure::kRings &&
           complete >= probesThroughRing(reducedThrough_)) {
        const std::uint32_t ring = reducedThrough_;

        std::uint32_t ringHits = 0;
        for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke) {
            ringBuffer_[spoke] = prober_.sample(pure::probeIndex(ring, spoke));
            if (ringBuffer_[spoke].hit) ++ringHits;
        }

        hits_ += ringHits;
        if (ringHits * 4 >= pure::kSpokes) reachRing_ = ring + 1;

        applyWaterLift();

        spikesDropped_ += pure::despikeRing(ringBuffer_, pure::kSpokes, ring);
        pure::smoothRing(ringBuffer_, pure::kSpokes, ring);

        if (ring >= 1) {
            for (std::uint32_t spoke = 0; spoke < pure::kSpokes;
                 spoke += pure::kRadialSpokeStride) {
                if (groundSegmentCount_ >= pure::kMaxGroundSegments) break;

                pure::ScanSegment produced{};
                pure::GroundRefusal why = pure::GroundRefusal::None;
                if (pure::trackedRadialStep(groundSpokeTracks_[spoke],
                                            previousRing_[spoke], ringBuffer_[spoke],
                                            ring, &produced,
                                            &why) != 0) {
                    groundSegments_[groundSegmentCount_++] = produced;
                } else if (previousRing_[spoke].hit && ringBuffer_[spoke].hit) {
                    const std::uint32_t neighbour =
                        spoke + 1u < pure::kSpokes ? spoke + 1u : spoke - 1u;
                    const bool recovered = pure::groundCellRecoversEdge(
                        previousRing_[spoke], ringBuffer_[spoke],
                        previousRing_[neighbour], ringBuffer_[neighbour], why);
                    if (recovered &&
                        pure::reduceRadialStep(
                            previousRing_[spoke], ringBuffer_[spoke], ring,
                            &produced, pure::GroundJoinRule::Recovered) != 0) {
                        groundSegments_[groundSegmentCount_++] = produced;
                        ++groundCellRecovered_;
                    } else {
                        ++groundRibRefused_;
                        noteRefusal(why);
                    }
                }
            }
        }

        const float rowSpacing = pure::sampleRowSpacing(ring);
        pure::GroundTrack arcTrack{};
        for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke)
            currentArcDrawn_[spoke] = false;
        for (std::uint32_t spoke = 0; spoke + 1 < pure::kSpokes; ++spoke) {
            if (groundSegmentCount_ >= pure::kMaxGroundSegments) break;
            const std::uint32_t nextSpoke = spoke + 1;

            pure::ScanSegment produced{};
            pure::GroundRefusal why = pure::GroundRefusal::None;
            if (pure::trackedArcStep(arcTrack, ringBuffer_[spoke], ringBuffer_[nextSpoke],
                                     ring, &produced, &why,
                                     rowSpacing) != 0) {
                groundSegments_[groundSegmentCount_++] = produced;
                currentArcDrawn_[spoke] = true;
            } else if (ringBuffer_[spoke].hit && ringBuffer_[nextSpoke].hit) {
                const bool recovered = ring >= 1u && pure::groundCellRecoversEdge(
                    ringBuffer_[spoke], ringBuffer_[nextSpoke],
                    previousRing_[spoke], previousRing_[nextSpoke], why);
                if (recovered &&
                    pure::reduceArcStep(ringBuffer_[spoke], ringBuffer_[nextSpoke],
                                        ring, &produced,
                                        pure::GroundJoinRule::Recovered) != 0) {
                    groundSegments_[groundSegmentCount_++] = produced;
                    currentArcDrawn_[spoke] = true;
                        ++groundCellRecovered_;
                } else {
                    ++groundArcRefused_;
                    noteRefusal(why);
                }
            }
        }

        for (std::uint32_t spoke = 0; spoke + 2 < pure::kSpokes; ++spoke) {
            if (groundSegmentCount_ >= pure::kMaxGroundSegments) break;
            if (!ringBuffer_[spoke].hit || ringBuffer_[spoke + 1u].hit ||
                !ringBuffer_[spoke + 2u].hit)
                continue;
            pure::ScanSegment healed{};
            if (pure::reduceArcStep(ringBuffer_[spoke], ringBuffer_[spoke + 2u],
                                    ring, &healed,
                                    pure::GroundJoinRule::Continuous) != 0) {
                groundSegments_[groundSegmentCount_++] = healed;
                currentArcDrawn_[spoke] = true;
                currentArcDrawn_[spoke + 1u] = true;
                ++groundHealed_;
            }
        }

        if (ring >= 2) emitRidgeCrests(ring);

        for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke) {
            ringTwoBack_[spoke] = previousRing_[spoke];
            previousRing_[spoke] = ringBuffer_[spoke];
            previousArcDrawn_[spoke] = currentArcDrawn_[spoke];
        }
        ++reducedThrough_;
    }

    diagnostics_.spikesDropped = spikesDropped_;
    diagnostics_.groundRefused = groundArcRefused_ + groundRibRefused_;
    diagnostics_.groundRaycasts = prober_.raycasts();
    diagnostics_.sampleTicks = prober_.sampleTicks();
    diagnostics_.groundRecovered = prober_.recovered();
    diagnostics_.groundCellRecovered = groundCellRecovered_;
    diagnostics_.groundHealed = groundHealed_;
    diagnostics_.waterLifted = waterLifted_;
    diagnostics_.crestArcs = crestArcs_;
    diagnostics_.hits = hits_;
    diagnostics_.reachRing = reachRing_;
    diagnostics_.reachMeters = reachRing_ == 0
                                   ? 0u
                                   : static_cast<std::uint32_t>(pure::ringRadius(reachRing_ - 1));

    rebuildWallSegments();
    composeSegments();

    diagnostics_.segments = segmentCount_;
}

void ScanController::resetGroundTracking() {
    for (auto& track : groundSpokeTracks_) track = pure::GroundTrack{};
    groundArcRefused_ = 0;
    groundRibRefused_ = 0;
    groundTrackRefused_ = 0;
    groundPairRefused_ = 0;
    groundCellRecovered_ = 0;
}

void ScanController::noteRefusal(pure::GroundRefusal why) {
    if (why == pure::GroundRefusal::Track) ++groundTrackRefused_;
    if (why == pure::GroundRefusal::Pairwise) ++groundPairRefused_;
}

void ScanController::resetWallTracking() {
    for (auto& tracks : wallColumnTracks_) tracks = pure::WallTrackSet{};
    wallRowsReduced_ = 0;
    wallHits_ = 0;
    wallCut_ = 0;
    wallBelowCount_ = 0;
    wallLowestOffset_ = 0.0f;

    wallRowEligible_ = 0;
    wallNoHit_ = 0;
    wallNotSteep_ = 0;
    wallHealed_ = 0;
    shallowAccepted_ = 0;
    shallowRedundant_ = 0;
    crestTops_ = 0;
    wallTopCrestEmitted_ = false;
}

void ScanController::rebuildWallSegments() {
    const std::uint32_t complete = wallProber_.completed();
    diagnostics_.wallProbesComplete = complete;
    diagnostics_.wallRaycasts = wallProber_.raycastsIssued();
    diagnostics_.wallSampleTicks = wallProber_.sampleTicks();
    if (pure::kWallColumns < 2 || pure::kWallRows < 2) {
        diagnostics_.wallSegments = 0;
        return;
    }

    const std::uint32_t completeRows = complete / pure::kWallColumns;
    const std::uint32_t usableRows =
        completeRows < pure::kWallRows ? completeRows : pure::kWallRows;
    if (usableRows <= wallRowsReduced_) return;

    const float originEye = originY_ + pure::kWallEyeHeight;
    pure::ScanSegment produced[pure::kWallDepthLayers]{};

    const auto append = [&](std::uint32_t producedCount) {
        for (std::uint32_t i = 0; i < producedCount; ++i) {
            if (wallSegmentCount_ >= pure::kMaxWallSegments) {
                ++wallCut_;
                continue;
            }
            wallSegments_[wallSegmentCount_++] = produced[i];
        }
    };

    for (std::uint32_t row = wallRowsReduced_; row < usableRows; ++row) {

        for (std::uint32_t column = 0; column < pure::kWallColumns; ++column) {
            const std::uint32_t index = row * pure::kWallColumns + column;
            const std::uint32_t count = wallProber_.sampleCount(index);

            if (count == 0) {
                const pure::TerrainSample& shallow =
                    wallProber_.shallowSample(index);
                if (shallow.hit) {
                    ++wallNotSteep_;
                } else {
                    ++wallNoHit_;
                }
            }
            for (std::uint32_t layer = 0; layer < count; ++layer) {
                const pure::TerrainSample& sample =
                    wallProber_.sample(index, layer);

                if (!pure::acceptedWallSample(sample)) continue;
                ++wallHits_;

                const float offset = sample.y - originEye;
                if (offset < -pure::kWallSubmergedMeters) ++wallBelowCount_;
                if (offset < wallLowestOffset_) wallLowestOffset_ = offset;
            }
        }

        pure::WallTrackSet acrossTracks{};
        for (std::uint32_t column = 0; column < pure::kWallColumns; ++column) {
            const std::uint32_t index = row * pure::kWallColumns + column;
            const std::uint32_t made = pure::advanceWallTracks(
                acrossTracks, wallProber_.samples(index),
                wallProber_.sampleCount(index), pure::WallEdgeAxis::Across,
                produced, pure::kWallDepthLayers);
            append(made);
        }

        {
            for (std::uint32_t column = 0; column + 2 < pure::kWallColumns;
                 ++column) {
                const std::uint32_t left = row * pure::kWallColumns + column;
                const std::uint32_t mid = left + 1u;
                const std::uint32_t right = left + 2u;
                if (wallProber_.sampleCount(mid) != 0) continue;
                if (wallProber_.sampleCount(left) == 0 ||
                    wallProber_.sampleCount(right) == 0)
                    continue;
                for (std::uint32_t layer = 0;
                     layer < pure::kWallDepthLayers; ++layer) {
                    if (wallSegmentCount_ >= pure::kMaxWallSegments) {
                        ++wallCut_;
                        break;
                    }
                    const pure::TerrainSample& a =
                        wallProber_.sample(left, layer);
                    const pure::TerrainSample& b =
                        wallProber_.sample(right, layer);
                    pure::ScanSegment healed{};
                    if (pure::reduceWallEdge(a, b, pure::WallEdgeAxis::Across,
                                             &healed, 2.0f) != 0) {
                        healed.origin = pure::wallOriginFor(layer);
                        healed.buried = a.buried && b.buried;
                        wallSegments_[wallSegmentCount_++] = healed;
                        ++wallHealed_;
                    }
                }
            }
        }

        emitShallowRow(row);

        for (std::uint32_t column = 0; column < pure::kWallColumns; ++column) {
            const std::uint32_t index = row * pure::kWallColumns + column;
            append(pure::advanceWallTracks(
                wallColumnTracks_[column], wallProber_.samples(index),
                wallProber_.sampleCount(index), pure::WallEdgeAxis::Vertical,
                produced, pure::kWallDepthLayers));
        }

    }

    wallRowsReduced_ = usableRows;

    if (wallProber_.complete() && wallIdentityJudged_ &&
        wallRowsReduced_ == pure::kWallRows && !wallTopCrestEmitted_) {
        emitWallTopCrests();
        wallTopCrestEmitted_ = true;
    }

    diagnostics_.wallHits = wallHits_;
    diagnostics_.wallSegments = wallSegmentCount_;
    diagnostics_.wallSegmentsDropped = wallCut_;
    diagnostics_.wallLowestDrop =
        wallLowestOffset_ < 0.0f ? static_cast<std::uint32_t>(-wallLowestOffset_)
                                 : 0u;
    diagnostics_.wallBelowSamples = wallBelowCount_;
    diagnostics_.wallNoHit = wallNoHit_;
    diagnostics_.wallNotSteep = wallNotSteep_;
    diagnostics_.wallHealed = wallHealed_;
    diagnostics_.shallowAccepted = shallowAccepted_;
    diagnostics_.shallowRedundant = shallowRedundant_;
    diagnostics_.crestTops = crestTops_;
}

void ScanController::emitRidgeCrests(std::uint32_t outerRing) {
    const std::uint32_t midRing = outerRing - 1u;
    const float spacing = pure::ringSpacing(midRing);
    bool ridge[pure::kSpokes];
    for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke)
        ridge[spoke] =
            pure::groundRidgePoint(ringTwoBack_[spoke], previousRing_[spoke],
                                   ringBuffer_[spoke], spacing);
    for (std::uint32_t spoke = 0; spoke + 1 < pure::kSpokes; ++spoke) {
        if (!ridge[spoke] || !ridge[spoke + 1u] || !previousArcDrawn_[spoke])
            continue;
        if (groundSegmentCount_ >= pure::kMaxGroundSegments) return;
        pure::ScanSegment crest{};
        if (pure::reduceArcStep(previousRing_[spoke], previousRing_[spoke + 1u],
                                midRing, &crest,
                                pure::GroundJoinRule::Continuous) != 0) {
            crest.crestline = true;
            groundSegments_[groundSegmentCount_++] = crest;
            ++crestArcs_;
        }
    }
}

void ScanController::emitWallTopCrests() {
    if (pure::kWallColumns < 2 || pure::kWallRows < 2) return;
    std::int32_t top[pure::kWallColumns];
    for (std::uint32_t column = 0; column < pure::kWallColumns; ++column) {
        top[column] = -1;
        const std::uint32_t edge = (pure::kWallRows - 1u) * pure::kWallColumns + column;
        if (wallProber_.sampleCount(edge) != 0 &&
            pure::acceptedWallSample(wallProber_.sample(edge, 0)))
            continue;
        for (std::int32_t row = static_cast<std::int32_t>(pure::kWallRows) - 2;
             row >= 0; --row) {
            const std::uint32_t index =
                static_cast<std::uint32_t>(row) * pure::kWallColumns + column;
            if (wallProber_.sampleCount(index) != 0 &&
                pure::acceptedWallSample(wallProber_.sample(index, 0))) {
                top[column] = row;
                break;
            }
        }
    }
    for (std::uint32_t column = 0; column + 1 < pure::kWallColumns; ++column) {
        if (top[column] < 0 || top[column + 1u] < 0) continue;
        const std::int32_t delta = top[column] - top[column + 1u];
        if ((delta < 0 ? -delta : delta) >
            static_cast<std::int32_t>(pure::kCrestTopRowTolerance))
            continue;
        if (wallSegmentCount_ >= pure::kMaxWallSegments) {
            ++wallCut_;
            return;
        }
        const pure::TerrainSample& a = wallProber_.sample(
            static_cast<std::uint32_t>(top[column]) * pure::kWallColumns + column,
            0);
        const pure::TerrainSample& b = wallProber_.sample(
            static_cast<std::uint32_t>(top[column + 1u]) * pure::kWallColumns +
                column + 1u,
            0);
        pure::ScanSegment crest{};
        if (pure::reduceWallEdge(a, b, pure::WallEdgeAxis::Across,
                                 &crest, 2.0f) != 0) {
            crest.crestline = true;
            crest.origin = pure::SegmentOrigin::WallNear;
            wallSegments_[wallSegmentCount_++] = crest;
            ++crestTops_;
        }
    }
}

void ScanController::emitShallowRow(std::uint32_t row) {
    for (std::uint32_t column = 0; column + 1 < pure::kWallColumns; ++column) {
        const std::uint32_t left = row * pure::kWallColumns + column;
        const std::uint32_t right = left + 1u;
        if (wallProber_.sampleCount(left) != 0 ||
            wallProber_.sampleCount(right) != 0)
            continue;
        const pure::TerrainSample& a = wallProber_.shallowSample(left);
        const pure::TerrainSample& b = wallProber_.shallowSample(right);
        if (!a.hit || !b.hit) continue;

        if (shallowRedundantWithGround(a) && shallowRedundantWithGround(b)) {
            ++shallowRedundant_;
            continue;
        }
        if (wallSegmentCount_ >= pure::kMaxWallSegments) {
            ++wallCut_;
            return;
        }
        pure::ScanSegment line{};
        if (pure::reduceShallowAcross(a, b, &line) != 0) {
            wallSegments_[wallSegmentCount_++] = line;
            ++shallowAccepted_;
        }
    }
}

bool ScanController::shallowRedundantWithGround(
    const pure::TerrainSample& sample) const {
    const float dx = sample.x - originX_;
    const float dz = sample.z - originZ_;
    const float radius = std::sqrt(dx * dx + dz * dz);
    if (!(radius > 0.1f)) return true;

    const float forward = dx * headingX_ + dz * headingZ_;
    const float side = dx * headingZ_ - dz * headingX_;
    const float wave = pure::waveCoordForRadius(forward);
    if (wave >= static_cast<float>(pure::kRings)) return false;
    std::int32_t ring = static_cast<std::int32_t>(wave + 0.5f) - 1;
    if (ring < 0) ring = 0;
    const float halfSpan = forward * pure::surveyTanHalfWidth();
    if (!(halfSpan > 0.001f)) return false;
    const float t = side / (2.0f * halfSpan) + 0.5f;
    const std::int32_t spoke = static_cast<std::int32_t>(
        t * static_cast<float>(pure::kSpokes - 1u) + 0.5f);
    if (spoke < 0 || spoke >= static_cast<std::int32_t>(pure::kSpokes)) return false;
    const pure::TerrainSample& ground = prober_.sample(
        pure::probeIndex(static_cast<std::uint32_t>(ring),
                         static_cast<std::uint32_t>(spoke)));
    if (!ground.hit) return false;

    const float dy = ground.y - sample.y;
    return (dy < 0.0f ? -dy : dy) <= pure::kShallowRedundancyMeters;
}

void ScanController::applyWaterLift() {
    for (std::uint32_t spoke = 0; spoke < pure::kSpokes; ++spoke) {
        pure::TerrainSample& sample = ringBuffer_[spoke];
        if (!sample.hit) continue;
        const auto water = waterProbe_.sample(sample.x, sample.z);
        if (!water.valid || !water.hasWater) continue;
        const float lift = water.surfaceY - sample.y;
        if (!(lift > 0.0f) || lift > kMaxWaterLiftMeters) continue;
        sample.y = water.surfaceY;
        sample.normalX = 0.0f;
        sample.normalY = 1.0f;
        sample.normalZ = 0.0f;
        ++waterLifted_;
    }
}

void ScanController::composeSegments() {

    for (std::uint32_t i = 0; i < groundSegmentCount_; ++i)
        groundSegments_[i].lane = pure::surveyLaneFor(groundSegments_[i]);
    for (std::uint32_t i = 0; i < wallSegmentCount_; ++i)
        wallSegments_[i].lane = pure::surveyLaneFor(wallSegments_[i]);

    buriedFiltered_ = 0;
    for (std::uint32_t i = 0; i < wallSegmentCount_; ++i)
        if (wallSegments_[i].buried && !pure::surveyBuriedShows(wallSegments_[i]))
            ++buriedFiltered_;

    for (std::uint32_t bucket = 0; bucket <= pure::kBatchBucketCount; ++bucket)
        sortCounts_[bucket] = 0;
    std::uint32_t* const counts = sortCounts_;
    for (std::uint32_t i = 0; i < groundSegmentCount_; ++i) {
        ++counts[pure::batchBucket(groundSegments_[i])];
    }
    for (std::uint32_t i = 0; i < wallSegmentCount_; ++i) {
        if (!pure::surveyBuriedShows(wallSegments_[i])) continue;
        ++counts[pure::batchBucket(wallSegments_[i])];
    }
    for (std::uint32_t bucket = 0; bucket <= pure::kBatchBucketCount; ++bucket)
        sortCursor_[bucket] = 0;
    std::uint32_t* const cursor = sortCursor_;
    std::uint32_t offset = 0;
    for (std::uint32_t bucket = 0; bucket <= pure::kBatchBucketCount; ++bucket) {
        cursor[bucket] = offset;
        offset += counts[bucket];
    }
    segmentCount_ =
        offset < pure::kMaxDrawnSegments ? offset : pure::kMaxDrawnSegments;
    diagnostics_.droppedSegments = offset - segmentCount_;

    const auto place = [&](const pure::ScanSegment& segment) {
        const std::uint32_t destination = cursor[pure::batchBucket(segment)]++;
        if (destination < pure::kMaxDrawnSegments)
            segments_[destination] = segment;
    };
    for (std::uint32_t i = 0; i < groundSegmentCount_; ++i)
        place(groundSegments_[i]);
    for (std::uint32_t i = 0; i < wallSegmentCount_; ++i) {
        if (!pure::surveyBuriedShows(wallSegments_[i])) continue;
        place(wallSegments_[i]);
    }

    render::publish(segments_, segmentCount_, tick_, scene_);
}

void ScanController::tick() {
    if (state_ == ScanState::Idle) return;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint32_t generation = 0;
    if (!resolveLink(x, y, z, generation)) {
        abandon(pure::ScanAbandonReason::PlayerLost);
        return;
    }
    if (generation != scene_) {
        abandon(pure::ScanAbandonReason::WorldReloaded);
        return;
    }

    ++tick_;

    if (state_ == ScanState::Pulsing) {

        prober_.authoriseTo(pure::authorisedProbeCount(tick_));
        {

            engine::perf::Timer rebuildTimer(engine::perf::gameRebuild);
            rebuildSegments();
        }

        if (tick_ >= pure::kPulseTicks &&
            prober_.completed() >= pure::kProbeCount && wallProber_.complete()) {
            prober_.disarm();
            state_ = ScanState::Holding;

            Logging.Log(
                "[zonai-survey] scan complete ticks=%u segments=%u dropped=%u "
                "spikes=%u hits=%u/%u reach=%um ring=%u/%u range_clipped=%u "
                "ground_rays=%u ground_ticks=%u",
                tick_, diagnostics_.segments, diagnostics_.droppedSegments,
                diagnostics_.spikesDropped, diagnostics_.hits, pure::kProbeCount,
                diagnostics_.reachMeters, diagnostics_.reachRing, pure::kRings,
                prober_.rangeClipped(), diagnostics_.groundRaycasts,
                static_cast<unsigned>(diagnostics_.sampleTicks));

            Logging.Log(
                "[zonai-survey] survey cliff probes=%u/%u rays=%u hits=%u "
                "segments=%u dropped=%u no_hit=%u not_steep=%u skipped=%u "
                "see_through=%u ticks=%u",
                diagnostics_.wallProbesComplete, pure::kWallProbes,
                diagnostics_.wallRaycasts, diagnostics_.wallHits,
                diagnostics_.wallSegments, diagnostics_.wallSegmentsDropped,
                diagnostics_.wallNoHit, diagnostics_.wallNotSteep,
                wallProber_.slopeSkipped(), wallProber_.seeThrough(),
                static_cast<unsigned>(diagnostics_.wallSampleTicks));

            Logging.Log(
                "[zonai-survey] survey depth below_eye=%um samples=%u | "
                "identity near=%u/%u mid=%u/%u far=%u/%u (surfaced/buried) "
                "hidden=%u drawn=%u",
                diagnostics_.wallLowestDrop, diagnostics_.wallBelowSamples,
                wallSurfaced_[0], wallBuried_[0], wallSurfaced_[1],
                wallBuried_[1], wallSurfaced_[2], wallBuried_[2],
                buriedFiltered_, buriedComposed_);

            Logging.Log(
                "[zonai-survey] survey continuity refused=%u track=%u pair=%u "
                "recovered=%u cell=%u | heals ground=%u cliff=%u shallow=%u "
                "redundant=%u water=%u crest_arcs=%u crest_tops=%u",
                diagnostics_.groundRefused, groundTrackRefused_,
                groundPairRefused_, diagnostics_.groundRecovered,
                diagnostics_.groundCellRecovered, diagnostics_.groundHealed,
                diagnostics_.wallHealed, diagnostics_.shallowAccepted,
                diagnostics_.shallowRedundant, diagnostics_.waterLifted,
                diagnostics_.crestArcs, diagnostics_.crestTops);
        }
    }

    if (pure::sweepExpired(tick_)) {
        abandon(pure::ScanAbandonReason::Expired);
        return;
    }

    render::publishTick(tick_);
}

void ScanController::serviceProbes(engine::RaycastFn original,
                                   const void* liveQueryObject) {
    if (state_ != ScanState::Pulsing) return;
    diagnostics_.probesIssued += prober_.service(original, liveQueryObject);
    diagnostics_.wallProbesIssued +=
        wallProber_.service(original, liveQueryObject);
}

}
