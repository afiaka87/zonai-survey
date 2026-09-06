// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace zonai_survey::pure {

struct SurveyAsset {
    std::string_view path;
    std::uint32_t bytes;
};

inline constexpr std::array<SurveyAsset, 8> kSurveyAssets{{
    {"Lib/sead/primitive_renderer/primitive_drawer_nvn_shader.bin", 7168},
    {"Lib/sead/nvn_font/nvn_font.ntx", 16956},
    {"Lib/sead/nvn_font/nvn_font_shader.bin", 8448},
    {"Lib/sead/nvn_font/nvn_font_jis1.ntx", 983612},
    {"Lib/sead/nvn_font/nvn_font_shader_jis1.bin", 9728},
    {"Lib/sead/nvn_font/nvn_font_shader_jis1_mipmap.bin", 9728},
    {"Lib/sead/nvn_font/nvn_font_jis1_tbl.bin", 7442},
    {"Lib/sead/nvn_font/nvn_font_jis1_mipmap.xtx", 3834428},
}};

inline constexpr std::string_view kContentAssetRoot = "content:/";
inline constexpr std::string_view kSdAssetRoot = "survey:/zonai-survey/";

enum class AssetSource { Unchecked, Content, SdCard, Unavailable };

constexpr AssetSource chooseAssetSource(bool contentComplete, bool sdComplete) {
    return contentComplete ? AssetSource::Content
                           : sdComplete ? AssetSource::SdCard : AssetSource::Unavailable;
}

constexpr const SurveyAsset* redirectedAsset(std::string_view path, bool readOnly,
                                            AssetSource source) {
    if (!readOnly || source != AssetSource::SdCard || !path.starts_with(kContentAssetRoot)) {
        return nullptr;
    }
    path.remove_prefix(kContentAssetRoot.size());
    for (const auto& asset : kSurveyAssets) {
        if (path == asset.path) return &asset;
    }
    return nullptr;
}

constexpr bool canBranchLink(std::intptr_t distance) {
    return distance % 4 == 0 && distance >= -0x08000000 && distance < 0x08000000;
}

}
