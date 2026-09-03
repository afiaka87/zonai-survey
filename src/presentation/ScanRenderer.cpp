// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include <atomic>
#include <cstdint>

#include <gfx/seadColor.h>

#include "ScanRenderer.hpp"

#include "PerfCounters.hpp"
#include "PerfReport.hpp"
#include "ScanBatching.hpp"
#include "SurveyPalette.hpp"
#include "totk/render/PrimitiveGeometry.hpp"
#include "totk/render/WorldDraw.hpp"

namespace zonai_survey::render {
namespace {

namespace seam = ::totk::render;

static_assert(pure::kRingBlockBytes ==
                  static_cast<std::uint32_t>(seam::kPrimitiveUniformAlign),
              "the Survey's ring budget must be written in the drawer's own blocks");

bool g_installed = false;


ScanFrame g_frames[2]{};
std::atomic<std::uint32_t> g_front{0};
std::atomic<std::uint32_t> g_tick{0};

std::atomic<std::uint32_t> g_lastDrawCalls{0};
std::atomic<std::uint32_t> g_lastDropped{0};

std::atomic<std::uint32_t> g_lastSegmentsDrawn{0};
std::atomic<bool> g_lastBatched{false};
std::atomic<std::uint32_t> g_lastRefused{0};

bool g_loggedRendered = false;
bool g_loggedNoVertexPath = false;
bool g_loggedVertexLive = false;

const ScanFrame* g_passFrame = nullptr;
std::uint32_t g_passTick = 0;
std::uint32_t g_passSegments = 0;

sead::Color4f lineTint(const sead::Color4f& base, float alpha) {
    return seam::lineTint(base, alpha);
}

sead::Color4f passColor(const pure::ScanSegment& segment, float alpha) {
    const pure::SurveyRgba color = pure::surveySegmentColor(segment, segment.revealWave);
    return lineTint(sead::Color4f{color.r, color.g, color.b, color.a}, alpha);
}

void passLaneColors(pure::SlopeLane lane, float revealWave, float alpha,
                    sead::Color4f& start, sead::Color4f& end) {
    const pure::SurveyRgba first = pure::surveyLaneColor(lane, false, revealWave);
    const pure::SurveyRgba second = pure::surveyLaneColor(lane, true, revealWave);
    start = lineTint(sead::Color4f{first.r, first.g, first.b, first.a}, alpha);
    end = lineTint(sead::Color4f{second.r, second.g, second.b, second.a}, alpha);
}

void writeBeamVertex(seam::VertexBlock& block, const seam::Vec3& point, float lift,
                     float rate, const seam::DepthLift& clearance) {
    const seam::Vec3 out =
        seam::applyDepthLift(clearance, seam::Vec3{point.x, point.y + lift, point.z});
    seam::writeVertex(block, out, rate);
}

std::uint8_t g_segStaticVisible[pure::kMaxDrawnSegments];
std::uint32_t g_groupStaticVisible[pure::kMaxSurveyGroups];
std::uint32_t g_visGeneration = 0xFFFFFFFFu;

void refreshVisibility(const ScanFrame& frame) {
    if (frame.generation == g_visGeneration) return;
    for (std::uint32_t g = 0; g < frame.groupCount; ++g) {
        const pure::GroupSpan span = frame.groups[g];
        std::uint32_t visible = 0;
        const std::uint32_t last = span.first + span.count;
        for (std::uint32_t i = span.first; i < last && i < pure::kMaxDrawnSegments; ++i) {
            const bool ok = pure::segmentStaticVisible(frame.segments[i]);
            g_segStaticVisible[i] = ok ? 1u : 0u;
            if (ok) ++visible;
        }
        g_groupStaticVisible[g] = visible;
    }
    g_visGeneration = frame.generation;
}

std::uint32_t passSegmentCount(const ScanFrame& frame, std::uint32_t tick) {
    std::uint32_t count = 0;
    for (std::uint32_t g = 0; g < frame.groupCount; ++g) {
        if (g_groupStaticVisible[g] == 0) continue;
        const pure::ScanSegment& head = frame.segments[frame.groups[g].first];
        if (!pure::groupTickPasses(head.revealWave, tick, head.crestline)) continue;
        count += g_groupStaticVisible[g];
    }
    return count;
}

std::uint32_t drawSurveyBeamBuffer(const ScanFrame& frame, std::uint32_t tick,
                                   std::uint32_t passSegments,
                                   const seam::VertexDrawPath& path,
                                   agl::DrawContext* drawCtx, const seam::EyePoint& eye,
                                   std::uint32_t& outConsidered,
                                   std::uint32_t& outRefused) {
    outConsidered = 0;
    outRefused = 0;
    const seam::DepthLift clearance =
        eye.valid ? seam::depthLiftFor(pure::kSurveyDepthLiftFraction, eye.x, eye.y, eye.z)
                  : seam::DepthLift{};

    seam::VertexBlock block =
        seam::reserveVertices(path, passSegments * seam::kBeamStripBudgetVertices);
    if (!block.valid) return 0;

    std::uint32_t draws = 0;
    float unusedWidth = -1.0f;
    for (std::uint32_t g = 0; g < frame.groupCount; ++g) {
        if (g_groupStaticVisible[g] == 0) continue;
        const pure::GroupSpan span = frame.groups[g];
        const pure::ScanSegment& head = frame.segments[span.first];
        if (!pure::groupTickPasses(head.revealWave, tick, head.crestline)) continue;

        const float alpha = pure::surveyPassAlpha(head.crestline, head.revealWave, tick);
        const float lift = pure::sweepLift(head.revealWave, tick);
        sead::Color4f startColor{};
        sead::Color4f endColor{};
        passLaneColors(head.lane, head.revealWave, alpha, startColor, endColor);
        if (pure::isCliffVertical(head)) {
            startColor.a *= pure::kCliffVerticalAlphaScale;
            endColor.a *= pure::kCliffVerticalAlphaScale;
        }

        const std::uint32_t first = block.written;
        std::uint32_t groupBeams = 0;
        const std::uint32_t last = span.first + span.count;
        for (std::uint32_t i = span.first; i < last && i < pure::kMaxDrawnSegments; ++i) {
            if (!g_segStaticVisible[i] || !frame.beams[i].valid) continue;
            const std::uint32_t needed =
                groupBeams == 0 ? seam::kBeamStripVertices : seam::kBeamStripBudgetVertices;
            if (block.written + needed > block.capacity) break;

            const float rate = pure::continuousSlopeRate(frame.segments[i].steepness,
                                                         frame.segments[i].slopeBand);
            if (groupBeams != 0) {
                block.cpu[block.written] = block.cpu[block.written - 1u];
                ++block.written;
                writeBeamVertex(block, frame.beams[i].vertices[0], lift, rate, clearance);
            }
            for (std::uint32_t v = 0; v < seam::kBeamStripVertices; ++v)
                writeBeamVertex(block, frame.beams[i].vertices[v], lift, rate, clearance);
            ++groupBeams;
        }

        const std::uint32_t vertices = block.written - first;
        if (groupBeams == 0 || vertices < seam::kBeamStripVertices) continue;
        if (seam::submitRange(path, drawCtx, first, vertices, startColor, endColor, 0.0f,
                              unusedWidth, seam::kPrimitiveTriangleStrip)) {
            ++draws;
            outConsidered += groupBeams;
        } else {
            ++outRefused;
        }
    }
    return draws;
}

std::uint32_t drawSurveyCylinderPass(const ScanFrame& frame, std::uint32_t tick,
                                     const seam::WorldFrame& world,
                                     std::uint32_t& outConsidered) {
    outConsidered = 0;
    std::uint32_t calls = 0;
    const seam::DepthLift clearance =
        world.eye.valid ? seam::depthLiftFor(pure::kSurveyDepthLiftFraction, world.eye.x,
                                             world.eye.y, world.eye.z)
                        : seam::DepthLift{};
    for (std::uint32_t i = 0; i < frame.count; ++i) {
        const pure::ScanSegment& segment = frame.segments[i];
        if (!pure::segmentPasses(segment, tick)) continue;
        const float alpha = pure::surveyPassAlpha(segment.crestline, segment.revealWave, tick);
        const float lift = pure::sweepLift(segment.revealWave, tick);
        const float width = pure::segmentWidth(segment.isArc);
        const sead::Color4f color = passColor(segment, alpha);
        if (seam::drawCylinderBar(
                world,
                seam::applyDepthLift(clearance,
                                     seam::Vec3{segment.ax, segment.ay + lift, segment.az}),
                seam::applyDepthLift(clearance,
                                     seam::Vec3{segment.bx, segment.by + lift, segment.bz}),
                width, color)) {
            ++calls;
            ++outConsidered;
        }
    }
    return calls;
}

#if OVERLAY_DEBUG_HUD
std::uint64_t g_drawPerfWindowStart = 0;
std::uint32_t g_drawPerfQuietWindows = 0;
std::uint64_t g_seamGfxTicksBase = 0;

void flushDrawPerf() {
    namespace perf = engine::perf;
    const std::uint64_t now = perf::now();
    const std::uint64_t freq = perf::frequency();
    if (g_drawPerfWindowStart == 0) {
        g_drawPerfWindowStart = now;
        g_seamGfxTicksBase = seam::stats().gfxTicks;
        return;
    }
    const std::uint64_t elapsed = now - g_drawPerfWindowStart;
    if (elapsed < freq) return;

    const std::uint64_t seamGfxTicks = seam::stats().gfxTicks;
    const std::uint64_t gfxWindow = seamGfxTicks - g_seamGfxTicksBase;
    const bool quiet = perf::drawFillOutline.count == 0 && perf::drawFillCore.count == 0 &&
                       perf::drawGlyphText.count == 0;
    const bool report = !quiet || (g_drawPerfQuietWindows++ % 10u) == 0;
    if (report) {
        using pure::perfMicros;
        using pure::perfMillis;
        Logging.Log(
            "[zonai-survey] perf draw %ums callbacks=%u active=%u survey=%uus "
            "vis=%uus gfx=%uus fill_o=%uus/%u fill_c=%uus/%u glyphs=%uus/%u "
            "skip_frame=%u hide=%u empty=%u",
            perfMillis(elapsed, freq), perf::drawGameplayCalls, perf::drawLayerCalls,
            perfMicros(perf::drawSurveyTotal.sum, freq), perfMicros(perf::drawVis.sum, freq),
            perfMicros(gfxWindow, freq), perfMicros(perf::drawFillOutline.sum, freq),
            perf::drawFillOutline.count, perfMicros(perf::drawFillCore.sum, freq),
            perf::drawFillCore.count, perfMicros(perf::drawGlyphText.sum, freq),
            perf::drawGlyphText.count, perf::drawSkipNoFrame, perf::drawSkipHidden,
            perf::drawSkipEmpty);
    }
    if (!quiet) g_drawPerfQuietWindows = 0;
    perf::drawSurveyTotal.reset();
    perf::drawVis.reset();
    perf::drawFillOutline.reset();
    perf::drawFillCore.reset();
    perf::drawGlyphText.reset();
    perf::drawLayerCalls = 0;
    perf::drawGameplayCalls = 0;
    perf::drawSkipNoFrame = 0;
    perf::drawSkipHidden = 0;
    perf::drawSkipEmpty = 0;
    g_seamGfxTicksBase = seamGfxTicks;
    g_drawPerfWindowStart = now;
}
#else
void flushDrawPerf() {}
#endif


bool surveyWantsDraw() {
    ++engine::perf::drawLayerCalls;
    flushDrawPerf();
    ++engine::perf::drawGameplayCalls;

    g_passFrame = nullptr;
    g_passSegments = 0;
    const ScanFrame& frame = g_frames[g_front.load(std::memory_order_acquire)];
    const std::uint32_t tick = g_tick.load(std::memory_order_acquire);
    if (frame.count == 0 || pure::scanExpired(tick)) {
        ++engine::perf::drawSkipNoFrame;
        return false;
    }

    engine::perf::Timer visTimer(engine::perf::drawVis);
    refreshVisibility(frame);
    const std::uint32_t segments = passSegmentCount(frame, tick);
    if (segments == 0) {
        ++engine::perf::drawSkipEmpty;
        return false;
    }
    g_passFrame = &frame;
    g_passTick = tick;
    g_passSegments = segments;
    return true;
}

void surveyDraw(const seam::WorldFrame& world) {
    if (!g_passFrame) return;
    const ScanFrame& frame = *g_passFrame;
    const bool onVertexPath = world.vertexPath != nullptr;

    std::uint32_t considered = 0;
    std::uint32_t surveyCalls = 0;
    std::uint32_t refused = 0;
    {
        engine::perf::Timer surveyTimer(engine::perf::drawSurveyTotal);
        engine::perf::Timer fillTimer(engine::perf::drawFillCore);
        if (onVertexPath) {
            surveyCalls = drawSurveyBeamBuffer(frame, g_passTick, g_passSegments,
                                               *world.vertexPath, world.drawContext,
                                               world.eye, considered, refused);
        } else {
            surveyCalls = drawSurveyCylinderPass(frame, g_passTick, world, considered);
            if (!g_loggedNoVertexPath) {
                g_loggedNoVertexPath = true;
                Logging.Log("[zonai-survey] batched beam path unavailable; "
                            "falling back to one native cylinder per segment");
            }
        }
    }

    const auto selection = pure::selectForDraw(considered);
    g_lastDrawCalls.store(surveyCalls, std::memory_order_relaxed);
    g_lastSegmentsDrawn.store(considered, std::memory_order_relaxed);
    g_lastBatched.store(onVertexPath, std::memory_order_relaxed);
    g_lastDropped.store(selection.dropped, std::memory_order_relaxed);
    g_lastRefused.store(refused, std::memory_order_relaxed);

    if (onVertexPath && !g_loggedVertexLive) {
        g_loggedVertexLive = true;
        Logging.Log("[zonai-survey] batched depth-tested beam submission live "
                    "stride=%d ring_cpu=%p ring_gpu=%llx",
                    seam::kPrimitiveVertexStride,
                    static_cast<void*>(world.vertexPath->ringCpu),
                    static_cast<unsigned long long>(world.vertexPath->ringGpu));
    }
    if (!g_loggedRendered) {
        g_loggedRendered = true;
        Logging.Log("[zonai-survey] depth-tested survey rendered segments=%u beams=%u "
                    "calls=%u dropped=%u vertex_path=%d",
                    frame.count, considered, surveyCalls, selection.dropped,
                    onVertexPath ? 1 : 0);
    }
}

}  

bool install(std::uintptr_t mainBase) {
    if (g_installed) return true;
    if (!seam::configure(mainBase)) {
        Logging.Log("[zonai-survey] shared world seam unavailable; nothing will be drawn "
                    "into the world this session");
        return false;
    }
    if (!seam::registerDrawer("zonai-survey/survey", surveyWantsDraw, surveyDraw, true)) {
        Logging.Log("[zonai-survey] world seam refused the survey drawer; nothing will be "
                    "drawn into the world this session");
        return false;
    }
    g_installed = true;
    Logging.Log("[zonai-survey] depth-tested beam drawer registered max_segments=%u",
                pure::kMaxDrawnSegments);
    return true;
}

void publish(const pure::ScanSegment* segments, std::uint32_t count, std::uint32_t tick,
             std::uint32_t sceneGeneration) {
    if (!g_installed) return;
    static std::uint32_t generation = 0;
    const std::uint32_t back = 1u - g_front.load(std::memory_order_relaxed);
    ScanFrame& frame = g_frames[back];
    if (count > pure::kMaxDrawnSegments) count = pure::kMaxDrawnSegments;
    frame.count = segments ? count : 0;
    frame.tick = tick;
    frame.scene = sceneGeneration;
    for (std::uint32_t i = 0; i < frame.count; ++i) {
        frame.segments[i] = segments[i];
        const pure::ScanSegment& segment = frame.segments[i];
        const std::uint32_t ring = segment.revealWave >= 1.0f
                                       ? static_cast<std::uint32_t>(segment.revealWave) - 1u
                                       : 0u;
        const float width = pure::segmentWidth(segment.isArc);
        const float radius = seam::primitiveRadius(width, pure::ringRadius(ring));
        frame.beams[i] = seam::buildSquareBeamStrip(
            seam::Vec3{segment.ax, segment.ay, segment.az},
            seam::Vec3{segment.bx, segment.by, segment.bz}, radius);
    }
    frame.groupCount =
        pure::buildGroupSpans(frame.segments, frame.count, frame.groups, pure::kMaxSurveyGroups);
    frame.generation = ++generation;
    g_tick.store(tick, std::memory_order_release);
    g_front.store(back, std::memory_order_release);
}

void publishTick(std::uint32_t tick) {
    if (!g_installed) return;
    g_tick.store(tick, std::memory_order_release);
}

void clear() {
    if (!g_installed) return;
    const std::uint32_t back = 1u - g_front.load(std::memory_order_relaxed);
    g_frames[back].count = 0;
    g_frames[back].tick = 0;
    g_front.store(back, std::memory_order_release);
}

bool lastSurveyBatched() {
    return g_lastBatched.load(std::memory_order_relaxed);
}

std::uint32_t lastSurveyLines() {
    return g_lastSegmentsDrawn.load(std::memory_order_relaxed);
}

std::uint32_t lastDrawCalls() {
    return g_lastDrawCalls.load(std::memory_order_relaxed);
}

std::uint32_t lastDroppedSegments() {
    return g_lastDropped.load(std::memory_order_relaxed);
}

std::uint32_t lastRefusedGroups() {
    return g_lastRefused.load(std::memory_order_relaxed);
}

}  
