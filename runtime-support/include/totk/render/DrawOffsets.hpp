// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include "totk/core/Units.hpp"

namespace totk::render {

struct DrawOffsets {
    static constexpr const char* kGameVersion = "1.2.1";
    static constexpr const char* kBuildId = "9B4E43650501A4D4";

    static constexpr totk::core::ImageOffset kGraphicsContextCtor{0x0074C19C};

    static constexpr totk::core::ImageOffset kGraphicsContextApply{0x00756A08};

    static constexpr totk::core::ImageOffset kBeginDrawImm{0x00DDC6AC};

    static constexpr totk::core::ImageOffset kDrawLineImm{0x00D977E8};

    static constexpr totk::core::ImageOffset kDrawTriangleImm{0x021F6350};

    static constexpr totk::core::ImageOffset kSetUniformBlock{0x0096D8A0};

    static constexpr totk::core::ImageOffset kSetVertexBlock{0x021F4FB8};

    static constexpr totk::core::ImageOffset kSetLineWidth{0x021F5078};

    static constexpr totk::core::ImageOffset kDrawModelSceneOpa{0x0248B4E4};

    static constexpr totk::core::ImageOffset kDrawQueue{0x00A43BE4};
};

struct PrimitiveSymbols {
    static constexpr const char* kRendererInstance = "sead::PrimitiveRenderer::instance";
    static constexpr const char* kBegin = "sead::PrimitiveDrawer::begin";
    static constexpr const char* kEnd = "sead::PrimitiveDrawer::end";
    static constexpr const char* kSetDrawCtx = "sead::PrimitiveDrawer::setDrawCtx";
    static constexpr const char* kSetCamera = "sead::PrimitiveDrawer::setCamera";
    static constexpr const char* kSetProjection = "sead::PrimitiveDrawer::setProjection";
    static constexpr const char* kSetModelMtx = "sead::PrimitiveDrawer::setModelMtx";
    static constexpr const char* kDrawSphere8x16 = "sead::PrimitiveDrawer::drawSphere8x16";
    static constexpr const char* kDrawCylinder16 = "sead::PrimitiveDrawer::drawCylinder16";
};

inline constexpr std::uintptr_t kImmDrawModuleSlot = 0x046381D8;

inline constexpr int kImmModuleProgramCount = 0x08;
inline constexpr int kImmModuleProgramTable = 0x10;
inline constexpr unsigned int kImmProgramCountMin = 0x3D;
inline constexpr int kImmLineProgram = 0x1E0;

inline constexpr unsigned int kImmColourSlot = 3;
inline constexpr unsigned int kImmVertexSlot = 4;

inline constexpr std::uintptr_t kLineWidthContextSlot = 0x0462EF90;

inline constexpr std::uintptr_t kDrawArraysSlot = 0x04616F28;
inline constexpr int kDrawContextCommandBuffer = 0xB8;
inline constexpr int kPrimitiveLines = 1;

inline constexpr int kPrimitiveTriangleStrip = 5;

inline constexpr int kDrawerWithinRenderer = 0x08;
inline constexpr std::uintptr_t kPrimitiveDrawMgrSlot = 0x0463CD48;
inline constexpr int kDrawMgrBlockExhausted = 1880;
inline constexpr int kDrawMgrCapacity = 1888;

inline constexpr std::uintptr_t kPrimitiveRingAlloc = 0x021C08E0;

inline constexpr int kDrawMgrRingBuffer = 1824;
inline constexpr int kDrawMgrRingCpuBase = 1872;
inline constexpr int kDrawMgrRingCursor = 1884;
inline constexpr int kDrawMgrRingRetire = 1892;

inline constexpr int kDrawMgrVertexAttribState = 440;
inline constexpr int kDrawMgrVertexStreamState = 456;

inline constexpr int kDrawMgrTexturedMode = 1900;

inline constexpr std::uintptr_t kNvnBufferGetAddressSlot = 0x04616F10;
inline constexpr std::uintptr_t kNvnBindVertexBufferSlot = 0x04616FD8;
inline constexpr std::uintptr_t kNvnBindUniformBufferSlot = 0x04617088;

inline constexpr int kPrimitiveVertexStride = 36;

inline constexpr int kPrimitiveDrawUniformBytes = 96;
inline constexpr int kPrimitiveUniformAlign = 256;
inline constexpr int kPrimitiveVertexStage = 0;
inline constexpr int kPrimitiveModelUniformIndex = 1;

inline constexpr float kPrimitiveMatrixLastRow[4] = {0.0f, 0.0f, 0.0f, 1.0f};

inline constexpr std::uintptr_t kOpaquePassEndBroadcast = 0x02AEFE74;

inline constexpr int kTranslucentQueue = 24;

inline constexpr int kSceneContextRenderBuffer = 0x540;

inline constexpr int kOpaquePassPhase = 0;

inline constexpr int kSceneContextViewIndex = 0x08;
inline constexpr int kSceneContextLayer = 0x5F0;

inline constexpr int kLayerCamera = 0x78;
inline constexpr int kLayerProjection = 0x80;
inline constexpr int kLayerViewSelectFlags = 0x8A;
inline constexpr int kLayerViewOverride = 0x1F0;
inline constexpr int kViewOverrideCamera = 0x08;
inline constexpr int kViewOverrideProjection = 0x160;
inline constexpr unsigned short kViewOverrideAvailable = 0x0021;
inline constexpr unsigned short kViewOverrideEnabled = 0x0002;

inline constexpr int kGcDepthTestEnable = 0x00;
inline constexpr int kGcDepthWriteEnable = 0x01;
inline constexpr int kGcDepthFunc = 0x64;
inline constexpr int kGcPolygonOffsetEnable = 0x6D;
inline constexpr int kGraphicsContextSize = 128;

inline constexpr unsigned char kGcDepthFuncNearerOrEqual = 4;

inline constexpr unsigned char kGcDepthFuncFarther = 5;
inline constexpr unsigned char kGcDepthFuncAlways = 8;

inline constexpr const char* kGameplayLayer = "Main_3D_0";

}
