// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

#include "PulseLattice.hpp"
#include "ScanBatching.hpp"
#include "ScanReduction.hpp"
#include "SurveyCue.hpp"
#include "SurveySweep.hpp"
#include "ScanVerdicts.hpp"
#include "TerrainProber.hpp"
#include "VerticalSurfaceProber.hpp"
#include "WallReduction.hpp"
#include "WaterSurfaceProbe.hpp"

namespace zonai_survey::feature {

enum class ScanState : std::uint8_t {
    Idle,
    Pulsing,  
    Holding,  
};

struct ScanDiagnostics {
    std::uint32_t probesIssued = 0;
    std::uint32_t probesComplete = 0;
    std::uint32_t segments = 0;
    std::uint32_t droppedSegments = 0;
    std::uint32_t spikesDropped = 0;
    std::uint32_t pulses = 0;
    std::uint32_t wallProbesIssued = 0;
    std::uint32_t wallProbesComplete = 0;
    std::uint32_t wallRaycasts = 0;
    std::uint64_t wallSampleTicks = 0;
    std::uint32_t wallHits = 0;
    std::uint32_t wallSegments = 0;
    std::uint32_t wallSegmentsDropped = 0;
    std::uint32_t wallLowestDrop = 0;
    std::uint32_t wallBelowSamples = 0;

    std::uint32_t groundRaycasts = 0;
    std::uint64_t sampleTicks = 0;

    std::uint32_t groundRefused = 0;
    std::uint32_t groundRecovered = 0;
    std::uint32_t groundCellRecovered = 0;

    std::uint32_t wallNoHit = 0;
    std::uint32_t wallNotSteep = 0;

    std::uint32_t groundHealed = 0;
    std::uint32_t wallHealed = 0;
    std::uint32_t shallowAccepted = 0;
    std::uint32_t shallowRedundant = 0;
    std::uint32_t waterLifted = 0;
    std::uint32_t crestArcs = 0;
    std::uint32_t crestTops = 0;

    std::uint32_t hits = 0;
    std::uint32_t reachRing = 0;    
    std::uint32_t reachMeters = 0;  

    pure::ScanVerdict lastVerdict = pure::ScanVerdict::Accepted;
    pure::ScanAbandonReason lastEnding = pure::ScanAbandonReason::Expired;
};

class ScanController {
  public:
    void initialize(std::uintptr_t mainBase);

    pure::ScanVerdict trigger();

    void tick();

    void serviceProbes(engine::RaycastFn original,
                       const void* liveQueryObject);

    [[nodiscard]] std::uint32_t pulseTicks() const { return tick_; }

    ScanState state() const { return state_; }
    const ScanDiagnostics& diagnostics() const { return diagnostics_; }

    float originX() const { return originX_; }
    float originY() const { return originY_; }
    float originZ() const { return originZ_; }
    std::uint32_t sceneGeneration() const { return scene_; }

    float headingX() const { return headingX_; }
    float headingZ() const { return headingZ_; }

  private:
    void abandon(pure::ScanAbandonReason reason);
    bool resolveLink(float& x, float& y, float& z, std::uint32_t& sceneGeneration);
    void rebuildSegments();
    void rebuildWallSegments();
    void resetWallTracking();
    void resetGroundTracking();
    void composeSegments();
    void emitRidgeCrests(std::uint32_t outerRing);
    void emitWallTopCrests();
    void emitShallowRow(std::uint32_t row);
    bool shallowRedundantWithGround(const pure::TerrainSample& sample) const;
    void applyWaterLift();
    void noteRefusal(pure::GroundRefusal why);

    std::uintptr_t mainBase_ = 0;
    ScanState state_ = ScanState::Idle;
    std::uint32_t tick_ = 0;
    std::uint32_t scene_ = 0;
    float originX_ = 0.0f;
    float originY_ = 0.0f;
    float originZ_ = 0.0f;
    float headingX_ = 0.0f;
    float headingZ_ = 1.0f;

    engine::TerrainProber prober_{};
    engine::VerticalSurfaceProber wallProber_{};
    ScanDiagnostics diagnostics_{};

    pure::ScanSegment groundSegments_[pure::kMaxGroundSegments]{};
    pure::ScanSegment wallSegments_[pure::kMaxWallSegments]{};
    pure::ScanSegment segments_[pure::kMaxDrawnSegments]{};
    std::uint32_t groundSegmentCount_ = 0;
    std::uint32_t wallSegmentCount_ = 0;
    std::uint32_t segmentCount_ = 0;
    std::uint32_t reducedThrough_ = 0;  

    pure::WallTrackSet wallColumnTracks_[pure::kWallColumns]{};
    pure::GroundTrack groundSpokeTracks_[pure::kSpokes]{};
    std::uint32_t groundArcRefused_ = 0;
    std::uint32_t groundRibRefused_ = 0;
    std::uint32_t groundTrackRefused_ = 0;
    std::uint32_t groundPairRefused_ = 0;
    std::uint32_t groundCellRecovered_ = 0;
    bool wallIdentityJudged_ = false;
    std::uint32_t wallSurfaced_[pure::kWallDepthLayers]{};
    std::uint32_t wallBuried_[pure::kWallDepthLayers]{};
    std::uint32_t sortCounts_[pure::kBatchBucketCount + 1u]{};
    std::uint32_t sortCursor_[pure::kBatchBucketCount + 1u]{};
    std::uint32_t wallRowEligible_ = 0;
    std::uint32_t wallNoHit_ = 0;
    std::uint32_t wallNotSteep_ = 0;
    std::uint32_t groundHealed_ = 0;
    std::uint32_t wallHealed_ = 0;
    std::uint32_t shallowAccepted_ = 0;
    std::uint32_t shallowRedundant_ = 0;
    std::uint32_t waterLifted_ = 0;
    std::uint32_t crestArcs_ = 0;
    std::uint32_t crestTops_ = 0;
    bool wallTopCrestEmitted_ = false;
    std::uint32_t buriedFiltered_ = 0;
    std::uint32_t buriedComposed_ = 0;
    std::uint32_t wallRowsReduced_ = 0;
    std::uint32_t wallHits_ = 0;
    std::uint32_t wallCut_ = 0;
    std::uint32_t wallBelowCount_ = 0;
    float wallLowestOffset_ = 0.0f;

    pure::TerrainSample ringBuffer_[pure::kSpokes]{};
    pure::TerrainSample previousRing_[pure::kSpokes]{};
    pure::TerrainSample ringTwoBack_[pure::kSpokes]{};
    bool previousArcDrawn_[pure::kSpokes]{};
    bool currentArcDrawn_[pure::kSpokes]{};
    engine::WaterSurfaceProbe waterProbe_{};
    std::uint32_t spikesDropped_ = 0;
    std::uint32_t hits_ = 0;
    std::uint32_t reachRing_ = 0;
};

}  
