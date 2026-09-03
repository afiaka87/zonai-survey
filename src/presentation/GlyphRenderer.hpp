// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <cstdint>

#include "GlyphLife.hpp"

namespace agl {
namespace lyr {
class Layer;
struct RenderInfo;
}
}

namespace lotuskit {
class TextWriterExt;
}

namespace zonai_survey::render {

struct GlyphDraw {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float distanceSq = 0.0f;  
    float alpha = 1.0f;
    std::uint16_t name = 0;
    std::uint16_t flags = 0;
    std::uint8_t cls = 0;
    std::uint8_t icon = 0;  
};

struct GlyphFrame {
    std::uint32_t count = 0;
    std::uint32_t scene = 0;
    GlyphDraw glyphs[pure::kMaxGlyphs]{};
};

void publishGlyphs(const GlyphFrame& frame);
void clearGlyphs();

void drawGlyphs(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info,
                lotuskit::TextWriterExt* writer);

void setSymbolProbeVisible(bool visible);
bool symbolProbeVisible();

std::uint32_t lastGlyphsDrawn();
std::uint32_t lastGlyphsOffscreen();
std::uint32_t lastNamesDropped();

}  
