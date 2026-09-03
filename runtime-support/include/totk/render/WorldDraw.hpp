// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

#include <gfx/seadColor.h>
#include <gfx/seadPrimitiveRenderer.h>

#include "totk/render/DrawOffsets.hpp"
#include "totk/render/PrimitiveGeometry.hpp"

namespace agl {
class DrawContext;
}

namespace sead {
class Camera;
class Projection;
}

namespace totk::render {

struct EyePoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool valid = false;
};

[[nodiscard]] float distanceTo(const EyePoint& eye, const Vec3& point);
[[nodiscard]] bool finite3(float x, float y, float z);

[[nodiscard]] sead::Color4f lineTint(const sead::Color4f& base, float alpha);

struct PrimitiveVertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float u = 0.0f, v = 0.0f;
    float rate[4]{};
};
static_assert(sizeof(PrimitiveVertex) == kPrimitiveVertexStride,
              "the drawer sizes its own buffer as 36 * count; this must match exactly");

struct VertexDrawPath {
    std::uintptr_t ringHeader = 0;
    std::uint8_t* ringCpu = nullptr;
    std::uint64_t ringGpu = 0;
    void* commandBuffer = nullptr;
    std::uintptr_t widthContext = 0;
    void* alloc = nullptr;
    void* bindVertexBuffer = nullptr;
    void* bindUniformBuffer = nullptr;
    void* drawArrays = nullptr;
    void* setLineWidth = nullptr;
    bool valid = false;
};

struct VertexBlock {
    PrimitiveVertex* cpu = nullptr;
    std::uint32_t offset = 0;
    std::uint32_t bytes = 0;
    std::uint32_t capacity = 0;  
    std::uint32_t written = 0;
    bool valid = false;
};

[[nodiscard]] bool ringExhausted(const VertexDrawPath& path);

[[nodiscard]] VertexBlock reserveVertices(const VertexDrawPath& path,
                                          std::uint32_t vertexCount);

bool submitRange(const VertexDrawPath& path, agl::DrawContext* drawContext,
                 std::uint32_t firstVertex, std::uint32_t vertexCount,
                 const sead::Color4f& startColor, const sead::Color4f& endColor,
                 float width, float& lastWidth, int primitive = kPrimitiveLines);

void writeVertex(VertexBlock& block, const Vec3& point, float rate);

struct WorldFrame {
    agl::DrawContext* drawContext = nullptr;
    sead::Camera* camera = nullptr;
    sead::Projection* projection = nullptr;
    sead::PrimitiveDrawer* drawer = nullptr;
    EyePoint eye{};
    const VertexDrawPath* vertexPath = nullptr;
};

bool drawSphereBar(const WorldFrame& frame, const Vec3& from, const Vec3& to,
                   float pixelWidth, const sead::Color4f& color);
bool drawCylinderBar(const WorldFrame& frame, const Vec3& from, const Vec3& to,
                     float pixelWidth, const sead::Color4f& color);

using WantsDrawFn = bool (*)();

using DrawFn = void (*)(const WorldFrame& frame);

inline constexpr int kMaxWorldDrawers = 4;

bool configure(std::uintptr_t mainBase);
[[nodiscard]] bool configured();

// Registration runs on the game thread and draw callbacks run on the render thread.
bool registerDrawer(const char* name, WantsDrawFn wants, DrawFn draw,
                    bool needsVertexPath = false);

struct SeamStats {
    std::uint32_t gameplayFrames = 0;  
    std::uint32_t openedPasses = 0;    
    std::uint32_t idleFrames = 0;      
    std::uint32_t refusedPasses = 0;   
    std::uint64_t gfxTicks = 0;        
    std::uint32_t gfxCalls = 0;
};
[[nodiscard]] SeamStats stats();
void resetStats();

}  
