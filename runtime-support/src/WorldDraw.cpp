// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#include <lib.hpp>

#include <cstdint>
#include <cstring>

#include <nn/util.h>  

#include <common/aglDrawContext.h>
#include <gfx/seadCamera.h>
#include <gfx/seadColor.h>
#include <gfx/seadPrimitiveRenderer.h>
#include <gfx/seadProjection.h>
#include <lyr/aglLayer.h>
#include <math/seadMatrix.h>
#include <math/seadVector.h>

#include "totk/render/WorldDraw.hpp"

#include "totk/engine/SymResolve.hpp"
#include "totk/render/DepthState.hpp"
#include "totk/ui/Diagnostics.hpp"
#include "totk/ui/OverlayLayout.hpp"

namespace totk::render {
namespace {


template <typename... Args>
void note(const char* format, Args... args) {
    char line[224];
    nn::util::SNPrintf(line, sizeof(line), format, args...);
    totk::ui::emitDiagnostic(line);
}


using VoidOnDrawer = void (*)(sead::PrimitiveDrawer*);
using SetDrawCtxFn = void (*)(sead::PrimitiveDrawer*, sead::DrawContext*);
using SetCameraFn = void (*)(sead::PrimitiveDrawer*, sead::Camera*);
using SetProjectionFn = void (*)(sead::PrimitiveDrawer*, sead::Projection*);
using SetModelMtxFn = void (*)(sead::PrimitiveDrawer*, sead::Matrix34f*);
using DrawSphereFn = void (*)(sead::PrimitiveDrawer*, const sead::Vector3f&, float,
                              const sead::Color4f&, const sead::Color4f&);
using DrawCylinderFn = void (*)(sead::PrimitiveDrawer*, const sead::Vector3f&, float,
                                float, const sead::Color4f&, const sead::Color4f&);

using SetLineWidthFn = void (*)(std::uintptr_t context, agl::DrawContext*, float width);

using DrawArraysFn = void (*)(void* commandBuffer, int mode, int first, int count);
using BufferGetAddressFn = std::uint64_t (*)(const void* nvnBuffer);
using BindVertexBufferFn = void (*)(void* commandBuffer, int index, std::uint64_t address,
                                    std::uint64_t size);
using BindUniformBufferFn = void (*)(void* commandBuffer, int stage, int index,
                                     std::uint64_t address, std::uint64_t size);
using RingAllocFn = std::uint32_t (*)(std::uintptr_t ringHeader, int byteCount);

sead::PrimitiveRenderer** g_rendererSlot = nullptr;
VoidOnDrawer g_begin = nullptr;
VoidOnDrawer g_end = nullptr;
SetDrawCtxFn g_setDrawCtx = nullptr;
SetCameraFn g_setCamera = nullptr;
SetProjectionFn g_setProjection = nullptr;
SetModelMtxFn g_setModelMtx = nullptr;
DrawSphereFn g_drawSphere = nullptr;
DrawCylinderFn g_drawCylinder = nullptr;

std::uintptr_t g_mainBase = 0;
bool g_configured = false;
bool g_hookInstalled = false;
sead::Matrix34f g_model;
const sead::Vector3f kLocalOrigin{0.0f, 0.0f, 0.0f};

bool g_loggedNoRenderer = false;
bool g_loggedImplausible = false;
bool g_loggedNoManager = false;
bool g_loggedExhausted = false;
bool g_loggedCapacity = false;
bool g_loggedTextured = false;
bool g_loggedRingShort = false;

constexpr std::uintptr_t kImageSpan = 0x10000000;

bool insideImage(std::uintptr_t pointer) {
    return pointer > g_mainBase && pointer < g_mainBase + kImageSpan;
}

std::uintptr_t drawManager() {
    if (!g_mainBase) return 0;
    const auto inner = *reinterpret_cast<std::uintptr_t*>(g_mainBase + kPrimitiveDrawMgrSlot);
    if (!inner || !insideImage(inner)) return 0;
    const auto manager = *reinterpret_cast<std::uintptr_t*>(inner);
    if (!manager) return 0;
    const auto vtable = *reinterpret_cast<std::uintptr_t*>(manager);
    return insideImage(vtable) ? manager : 0;
}

void noteExhausted(std::uintptr_t manager) {
    if (!manager || g_loggedExhausted) return;
    if (*reinterpret_cast<std::uint8_t*>(manager + kDrawMgrBlockExhausted) != 0) {
        g_loggedExhausted = true;
        note("[totk-render] primitive drawer uniform block exhausted; later shapes vanished");
    }
}

void noteCapacity(std::uintptr_t manager) {
    if (!manager || g_loggedCapacity) return;
    g_loggedCapacity = true;
    const auto bytes = *reinterpret_cast<std::uint32_t*>(manager + kDrawMgrCapacity);
    note("[totk-render] primitive drawer capacity bytes=%u allocations=%u", bytes,
         bytes / totk::ui::tuning::kPrimitiveUniformBlockBytes);
}

sead::PrimitiveDrawer* readyDrawer() {
    if (!g_configured || !g_rendererSlot) return nullptr;
    auto* const renderer = *g_rendererSlot;
    if (!renderer) {
        if (!g_loggedNoRenderer) {
            g_loggedNoRenderer = true;
            note("[totk-render] primitive renderer was not created; check shipped shader");
        }
        return nullptr;
    }

    const auto rendererAddress = reinterpret_cast<std::uintptr_t>(renderer);
    const auto drawerAddress = rendererAddress + kDrawerWithinRenderer;
    const auto rendererVtable = *reinterpret_cast<std::uintptr_t*>(rendererAddress);
    const auto drawerVtable = *reinterpret_cast<std::uintptr_t*>(drawerAddress);
    if (!insideImage(rendererVtable) || !insideImage(drawerVtable)) {
        if (!g_loggedImplausible) {
            g_loggedImplausible = true;
            note("[totk-render] primitive renderer layout refused vtables=%p/%p",
                 reinterpret_cast<void*>(rendererVtable),
                 reinterpret_cast<void*>(drawerVtable));
        }
        return nullptr;
    }

    const auto manager = drawManager();
    if (!manager) {
        if (!g_loggedNoManager) {
            g_loggedNoManager = true;
            note("[totk-render] primitive drawer manager unavailable; begin would fault");
        }
        return nullptr;
    }
    noteCapacity(manager);
    noteExhausted(manager);
    return reinterpret_cast<sead::PrimitiveDrawer*>(drawerAddress);
}

EyePoint eyeFrom(const sead::Matrix34f& view) {
    EyePoint eye{};
    const float tx = view.m[0][3];
    const float ty = view.m[1][3];
    const float tz = view.m[2][3];
    eye.x = -(view.m[0][0] * tx + view.m[1][0] * ty + view.m[2][0] * tz);
    eye.y = -(view.m[0][1] * tx + view.m[1][1] * ty + view.m[2][1] * tz);
    eye.z = -(view.m[0][2] * tx + view.m[1][2] * ty + view.m[2][2] * tz);
    eye.valid = __builtin_isfinite(eye.x) && __builtin_isfinite(eye.y) &&
                __builtin_isfinite(eye.z);
    return eye;
}

void setIdentityModel() {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) g_model.m[row][col] = 0.0f;
        g_model.m[row][row] = 1.0f;
    }
}

void setSphereModel(const PrimitiveBar& bar) {
    g_model.m[0][0] = bar.right.x * bar.radius;
    g_model.m[1][0] = bar.right.y * bar.radius;
    g_model.m[2][0] = bar.right.z * bar.radius;
    g_model.m[0][1] = bar.up.x * bar.radius;
    g_model.m[1][1] = bar.up.y * bar.radius;
    g_model.m[2][1] = bar.up.z * bar.radius;
    g_model.m[0][2] = bar.axis.x * bar.halfLength;
    g_model.m[1][2] = bar.axis.y * bar.halfLength;
    g_model.m[2][2] = bar.axis.z * bar.halfLength;
    g_model.m[0][3] = bar.center.x;
    g_model.m[1][3] = bar.center.y;
    g_model.m[2][3] = bar.center.z;
}

void setCylinderModel(const PrimitiveBar& bar) {
    g_model.m[0][0] = bar.right.x;
    g_model.m[1][0] = bar.right.y;
    g_model.m[2][0] = bar.right.z;
    g_model.m[0][1] = bar.axis.x;
    g_model.m[1][1] = bar.axis.y;
    g_model.m[2][1] = bar.axis.z;
    g_model.m[0][2] = bar.up.x;
    g_model.m[1][2] = bar.up.y;
    g_model.m[2][2] = bar.up.z;
    g_model.m[0][3] = bar.center.x;
    g_model.m[1][3] = bar.center.y;
    g_model.m[2][3] = bar.center.z;
}

PrimitiveBar makeBar(const Vec3& from, const Vec3& to, float pixelWidth,
                     const EyePoint& eye) {
    const Vec3 center{(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f,
                      (from.z + to.z) * 0.5f};
    const float radius = primitiveRadius(pixelWidth, distanceTo(eye, center));
    return buildPrimitiveBar(from, to, radius);
}


bool plausiblePointer(std::uintptr_t pointer) {
    return pointer > 0x1000 && (pointer & 0x7) == 0;
}

void* resolveDriverFn(std::uintptr_t slot) {
    if (!g_mainBase) return nullptr;
    const auto entry = *reinterpret_cast<std::uintptr_t*>(g_mainBase + slot);
    if (!plausiblePointer(entry)) return nullptr;
    const auto fn = *reinterpret_cast<std::uintptr_t*>(entry);
    if (!plausiblePointer(fn)) return nullptr;
    return reinterpret_cast<void*>(fn);
}

VertexDrawPath resolveVertexPath(agl::DrawContext* drawCtx) {
    VertexDrawPath path{};
    const auto manager = drawManager();
    if (!manager || !drawCtx || !g_mainBase) return path;

    if (*reinterpret_cast<std::uint8_t*>(manager + kDrawMgrTexturedMode) != 0) {
        if (!g_loggedTextured) {
            g_loggedTextured = true;
            note("[totk-render] primitive drawer is in textured mode; the batched "
                 "vertex path declined and the immediate path drew");
        }
        return path;
    }

    path.ringCpu = *reinterpret_cast<std::uint8_t**>(manager + kDrawMgrRingCpuBase);
    if (!plausiblePointer(reinterpret_cast<std::uintptr_t>(path.ringCpu))) return path;

    auto* const getAddress = resolveDriverFn(kNvnBufferGetAddressSlot);
    path.bindVertexBuffer = resolveDriverFn(kNvnBindVertexBufferSlot);
    path.bindUniformBuffer = resolveDriverFn(kNvnBindUniformBufferSlot);
    path.drawArrays = resolveDriverFn(kDrawArraysSlot);
    if (!getAddress || !path.bindVertexBuffer || !path.bindUniformBuffer ||
        !path.drawArrays) {
        return path;
    }

    path.ringGpu = reinterpret_cast<BufferGetAddressFn>(getAddress)(
        reinterpret_cast<const void*>(manager + kDrawMgrRingBuffer));
    if (path.ringGpu == 0) return path;

    path.commandBuffer = *reinterpret_cast<void**>(
        reinterpret_cast<std::uintptr_t>(drawCtx) + kDrawContextCommandBuffer);
    if (!path.commandBuffer) return path;

    const auto widthSlot = *reinterpret_cast<std::uintptr_t*>(g_mainBase + kLineWidthContextSlot);
    if (!plausiblePointer(widthSlot)) return path;
    path.widthContext = *reinterpret_cast<std::uintptr_t*>(widthSlot);

    path.alloc = reinterpret_cast<void*>(g_mainBase + kPrimitiveRingAlloc);
    path.setLineWidth = reinterpret_cast<void*>(g_mainBase + DrawOffsets::kSetLineWidth.value);
    path.ringHeader = manager + kDrawMgrBlockExhausted;
    path.valid = true;
    return path;
}

constexpr std::uint32_t roundToRingBlock(std::uint32_t bytes) {
    constexpr std::uint32_t kBlock = static_cast<std::uint32_t>(kPrimitiveUniformAlign);
    return ((bytes + kBlock - 1u) / kBlock) * kBlock;
}

void noteRingShort() {
    if (g_loggedRingShort) return;
    g_loggedRingShort = true;
    note("[totk-render] primitive drawer ring ran out mid-frame; the remaining draws "
         "were not submitted. Raise the configured ring size.");
}


struct Registration {
    const char* name = nullptr;
    WantsDrawFn wants = nullptr;
    DrawFn draw = nullptr;
    bool needsVertexPath = false;
};

Registration g_drawers[kMaxWorldDrawers]{};
int g_drawerCount = 0;
SeamStats g_stats{};

class GfxTimer {
  public:
    GfxTimer() {
#if OVERLAY_DEBUG_HUD
        start_ = svcGetSystemTick();
#endif
    }
    ~GfxTimer() {
#if OVERLAY_DEBUG_HUD
        g_stats.gfxTicks += svcGetSystemTick() - start_;
        ++g_stats.gfxCalls;
#endif
    }
    GfxTimer(const GfxTimer&) = delete;
    GfxTimer& operator=(const GfxTimer&) = delete;

#if OVERLAY_DEBUG_HUD
  private:
    std::uint64_t start_ = 0;
#endif
};


void dispatch(agl::DrawContext* drawCtx, sead::Camera* camera,
              sead::Projection* projection) {
    if (!g_configured || !drawCtx || !camera || !projection) return;
    ++g_stats.gameplayFrames;

    bool wants[kMaxWorldDrawers]{};
    bool anyWants = false;
    bool anyVertexPath = false;
    for (int i = 0; i < g_drawerCount; ++i) {
        wants[i] = g_drawers[i].wants == nullptr || g_drawers[i].wants();
        if (!wants[i]) continue;
        anyWants = true;
        anyVertexPath = anyVertexPath || g_drawers[i].needsVertexPath;
    }
    if (!anyWants) {
        ++g_stats.idleFrames;
        return;
    }

    sead::PrimitiveDrawer* const drawer = readyDrawer();
    if (!drawer || !applyDepthTestedOverlay(drawCtx)) {
        ++g_stats.refusedPasses;
        return;
    }

    const VertexDrawPath path = anyVertexPath ? resolveVertexPath(drawCtx) : VertexDrawPath{};

    WorldFrame frame{};
    frame.drawContext = drawCtx;
    frame.camera = camera;
    frame.projection = projection;
    frame.drawer = drawer;
    frame.eye = eyeFrom(camera->getMatrix());

    {
        GfxTimer timer;
        setIdentityModel();
        g_setProjection(drawer, projection);
        g_setCamera(drawer, camera);
        g_setModelMtx(drawer, &g_model);
        g_setDrawCtx(drawer, drawCtx);
        g_begin(drawer);
    }

    for (int i = 0; i < g_drawerCount; ++i) {
        if (!wants[i] || g_drawers[i].draw == nullptr) continue;
        frame.vertexPath = g_drawers[i].needsVertexPath && path.valid ? &path : nullptr;
        g_drawers[i].draw(frame);
    }

    {
        GfxTimer timer;
        g_end(drawer);
    }
    noteExhausted(drawManager());
    restoreSceneState(drawCtx);
    ++g_stats.openedPasses;
}


struct LayerView {
    sead::Camera* camera = nullptr;
    sead::Projection* projection = nullptr;
};

int g_drawViewIndex = -1;
bool g_loggedSecondView = false;
bool g_loggedNoCamera = false;
bool g_loggedSeam = false;
bool g_loggedNulls = false;

template <typename T>
T fieldAt(const void* base, int offset) {
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return value;
}

bool selectLayerView(const void* layer, LayerView& out) {
    const auto flags = fieldAt<unsigned short>(layer, kLayerViewSelectFlags);
    auto* const viewOverride = fieldAt<std::uint8_t*>(layer, kLayerViewOverride);
    const bool useOverride = (flags & kViewOverrideAvailable) != 0 &&
                             (flags & kViewOverrideEnabled) != 0 && viewOverride != nullptr;
    if (useOverride) {
        out.camera = reinterpret_cast<sead::Camera*>(viewOverride + kViewOverrideCamera);
        out.projection = fieldAt<sead::Projection*>(viewOverride, kViewOverrideProjection);
    } else {
        out.camera = fieldAt<sead::Camera*>(layer, kLayerCamera);
        out.projection = fieldAt<sead::Projection*>(layer, kLayerProjection);
    }
    return out.camera && out.projection;
}

bool gameplayViewAt(const void* sceneContext, LayerView& out) {
    auto* const layer = fieldAt<agl::lyr::Layer*>(sceneContext, kSceneContextLayer);
    const char* const name = layer ? layer->mLayerName.cstr() : nullptr;
    if (!name || std::strcmp(name, kGameplayLayer) != 0) return false;

    const int view = fieldAt<std::uint8_t>(sceneContext, kSceneContextViewIndex);
    if (g_drawViewIndex < 0) {
        g_drawViewIndex = view;
    } else if (view != g_drawViewIndex) {
        if (!g_loggedSecondView) {
            g_loggedSecondView = true;
            note("[totk-render] %s also rendered in view %d; the seam stays in %d",
                 kGameplayLayer, view, g_drawViewIndex);
        }
        return false;
    }

    if (!selectLayerView(layer, out)) {
        if (!g_loggedNoCamera) {
            g_loggedNoCamera = true;
            note("[totk-render] end-of-pass draw has no gameplay camera/projection");
        }
        return false;
    }
    return true;
}

void onOpaquePassEnd(agl::DrawContext* drawCtx, const void* sceneContext) {
    if (!drawCtx || !sceneContext) {
        if (!g_loggedNulls) {
            g_loggedNulls = true;
            note("[totk-render] end-of-pass seam refused context=%p scene=%p",
                 static_cast<void*>(drawCtx), sceneContext);
        }
        return;
    }

    LayerView view{};
    if (!gameplayViewAt(sceneContext, view)) return;
    if (!g_loggedSeam) {
        g_loggedSeam = true;
        note("[totk-render] depth-correct world seam live: %s view %d", kGameplayLayer,
             g_drawViewIndex);
    }
    dispatch(drawCtx, view.camera, view.projection);
}

HOOK_DEFINE_TRAMPOLINE(OpaquePassEndHook) {
    static u64 Callback(void* self, agl::DrawContext* drawCtx, int phase, void* scene,
                        void* sceneContext) {
        const u64 result = Orig(self, drawCtx, phase, scene, sceneContext);
        onOpaquePassEnd(drawCtx, sceneContext);
        return result;
    }
};

}  


float distanceTo(const EyePoint& eye, const Vec3& point) {
    if (!eye.valid) return 0.0f;
    const float dx = point.x - eye.x;
    const float dy = point.y - eye.y;
    const float dz = point.z - eye.z;
    return __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
}

bool finite3(float x, float y, float z) {
    return __builtin_isfinite(x) && __builtin_isfinite(y) && __builtin_isfinite(z);
}

sead::Color4f lineTint(const sead::Color4f& base, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return sead::Color4f{base.r, base.g, base.b, base.a * __builtin_sqrtf(alpha)};
}

bool drawSphereBar(const WorldFrame& frame, const Vec3& from, const Vec3& to,
                   float pixelWidth, const sead::Color4f& color) {
    if (!frame.drawer || !g_drawSphere) return false;
    const PrimitiveBar bar = makeBar(from, to, pixelWidth, frame.eye);
    if (!bar.valid) return false;
    setSphereModel(bar);
    g_drawSphere(frame.drawer, kLocalOrigin, 1.0f, color, color);
    return true;
}

bool drawCylinderBar(const WorldFrame& frame, const Vec3& from, const Vec3& to,
                     float pixelWidth, const sead::Color4f& color) {
    if (!frame.drawer || !g_drawCylinder) return false;
    const PrimitiveBar bar = makeBar(from, to, pixelWidth, frame.eye);
    if (!bar.valid) return false;
    setCylinderModel(bar);
    g_drawCylinder(frame.drawer, kLocalOrigin, bar.radius, bar.halfLength * 2.0f, color,
                   color);
    return true;
}

bool ringExhausted(const VertexDrawPath& path) {
    return path.ringHeader != 0 && *reinterpret_cast<std::uint8_t*>(path.ringHeader) != 0;
}

VertexBlock reserveVertices(const VertexDrawPath& path, std::uint32_t vertexCount) {
    VertexBlock block{};
    if (!path.valid || vertexCount < 2) return block;
    if (ringExhausted(path)) {
        noteRingShort();
        return block;
    }

    block.bytes = vertexCount * static_cast<std::uint32_t>(kPrimitiveVertexStride);
    block.offset = reinterpret_cast<RingAllocFn>(path.alloc)(
        path.ringHeader, static_cast<int>(roundToRingBlock(block.bytes)));
    if (ringExhausted(path)) {
        noteRingShort();
        return block;
    }

    block.cpu = reinterpret_cast<PrimitiveVertex*>(path.ringCpu + block.offset);
    block.capacity = vertexCount;
    reinterpret_cast<BindVertexBufferFn>(path.bindVertexBuffer)(
        path.commandBuffer, 0, path.ringGpu + block.offset, block.bytes);
    block.valid = true;
    return block;
}

bool submitRange(const VertexDrawPath& path, agl::DrawContext* drawContext,
                 std::uint32_t firstVertex, std::uint32_t vertexCount,
                 const sead::Color4f& startColor, const sead::Color4f& endColor,
                 float width, float& lastWidth, int primitive) {
    if (!path.valid || vertexCount < 2) return false;
    if (ringExhausted(path)) {
        noteRingShort();
        return false;
    }

    const std::uint32_t uniformOffset =
        reinterpret_cast<RingAllocFn>(path.alloc)(path.ringHeader, kPrimitiveUniformAlign);
    if (ringExhausted(path)) {
        noteRingShort();
        return false;
    }

    auto* const block = reinterpret_cast<float*>(path.ringCpu + uniformOffset);
    for (int i = 0; i < 12; ++i) block[i] = 0.0f;
    block[0] = 1.0f;
    block[5] = 1.0f;
    block[10] = 1.0f;
    for (int i = 0; i < 4; ++i) block[12 + i] = kPrimitiveMatrixLastRow[i];
    block[16] = startColor.r; block[17] = startColor.g;
    block[18] = startColor.b; block[19] = startColor.a;
    block[20] = endColor.r; block[21] = endColor.g;
    block[22] = endColor.b; block[23] = endColor.a;

    reinterpret_cast<BindUniformBufferFn>(path.bindUniformBuffer)(
        path.commandBuffer, kPrimitiveVertexStage, kPrimitiveModelUniformIndex,
        path.ringGpu + uniformOffset, kPrimitiveDrawUniformBytes);
    if (primitive == kPrimitiveLines && width != lastWidth) {
        reinterpret_cast<SetLineWidthFn>(path.setLineWidth)(path.widthContext, drawContext,
                                                            width);
        lastWidth = width;
    }
    reinterpret_cast<DrawArraysFn>(path.drawArrays)(path.commandBuffer, primitive,
                                                    static_cast<int>(firstVertex),
                                                    static_cast<int>(vertexCount));
    return true;
}

void writeVertex(VertexBlock& block, const Vec3& point, float rate) {
    PrimitiveVertex& vertex = block.cpu[block.written++];
    vertex.x = point.x;
    vertex.y = point.y;
    vertex.z = point.z;
    vertex.u = 0.0f;
    vertex.v = 0.0f;
    vertex.rate[0] = rate;
    vertex.rate[1] = rate;
    vertex.rate[2] = rate;
    vertex.rate[3] = rate;
}

bool configure(std::uintptr_t mainBase) {
    if (g_configured) return true;
    g_mainBase = mainBase;
    g_model = sead::Matrix34f{1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f};

    g_rendererSlot =
        EXL_SYM_RESOLVE<sead::PrimitiveRenderer**>(PrimitiveSymbols::kRendererInstance);
    g_begin = EXL_SYM_RESOLVE<VoidOnDrawer>(PrimitiveSymbols::kBegin);
    g_end = EXL_SYM_RESOLVE<VoidOnDrawer>(PrimitiveSymbols::kEnd);
    g_setDrawCtx = EXL_SYM_RESOLVE<SetDrawCtxFn>(PrimitiveSymbols::kSetDrawCtx);
    g_setCamera = EXL_SYM_RESOLVE<SetCameraFn>(PrimitiveSymbols::kSetCamera);
    g_setProjection = EXL_SYM_RESOLVE<SetProjectionFn>(PrimitiveSymbols::kSetProjection);
    g_setModelMtx = EXL_SYM_RESOLVE<SetModelMtxFn>(PrimitiveSymbols::kSetModelMtx);
    g_drawSphere = EXL_SYM_RESOLVE<DrawSphereFn>(PrimitiveSymbols::kDrawSphere8x16);
    g_drawCylinder = EXL_SYM_RESOLVE<DrawCylinderFn>(PrimitiveSymbols::kDrawCylinder16);

    const bool symbolsReady = g_rendererSlot && g_begin && g_end && g_setDrawCtx &&
                              g_setCamera && g_setProjection && g_setModelMtx &&
                              g_drawSphere && g_drawCylinder;
    if (!symbolsReady || !configureDepthState(mainBase)) {
        note("[totk-render] world seam setup incomplete slot=%p begin=%p end=%p sphere=%p "
             "cylinder=%p",
             static_cast<void*>(g_rendererSlot), reinterpret_cast<void*>(g_begin),
             reinterpret_cast<void*>(g_end), reinterpret_cast<void*>(g_drawSphere),
             reinterpret_cast<void*>(g_drawCylinder));
        return false;
    }

    g_configured = true;
    if (!g_hookInstalled) {
        g_hookInstalled = true;
        OpaquePassEndHook::InstallAtOffset(kOpaquePassEndBroadcast);
        note("[totk-render] depth-correct world seam configured and installed");
    }
    return true;
}

bool configured() { return g_configured; }

bool registerDrawer(const char* name, WantsDrawFn wants, DrawFn draw,
                    bool needsVertexPath) {
    if (!name || !draw) return false;
    for (int i = 0; i < g_drawerCount; ++i) {
        if (std::strcmp(g_drawers[i].name, name) == 0) {
            note("[totk-render] world drawer '%s' is already registered", name);
            return false;
        }
    }
    if (g_drawerCount >= kMaxWorldDrawers) {
        note("[totk-render] world seam full (%d drawers); '%s' will not draw",
             kMaxWorldDrawers, name);
        return false;
    }
    g_drawers[g_drawerCount] = Registration{name, wants, draw, needsVertexPath};
    ++g_drawerCount;
    note("[totk-render] world drawer '%s' registered (slot %d, batched=%d)", name,
         g_drawerCount - 1, needsVertexPath ? 1 : 0);
    return true;
}

SeamStats stats() { return g_stats; }

void resetStats() { g_stats = SeamStats{}; }

}  
