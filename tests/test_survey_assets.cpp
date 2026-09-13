// SPDX-License-Identifier: GPL-2.0-only
#include <doctest.h>
#include <string>
#include "SurveyAssets.hpp"

using namespace zonai_survey::pure;

TEST_CASE("Asset selection requires a whole source and preserves content precedence") {
    CHECK(chooseAssetSource(true, true) == AssetSource::Content);
    CHECK(chooseAssetSource(true, false) == AssetSource::Content);
    CHECK(chooseAssetSource(false, true) == AssetSource::SdCard);
    CHECK(chooseAssetSource(false, false) == AssetSource::Unavailable);
}

TEST_CASE("Only the eight exact read-only content assets can redirect") {
    std::uint32_t bytes = 0;
    for (const auto& asset : kSurveyAssets) {
        const std::string path = std::string(kContentAssetRoot) + std::string(asset.path);
        CHECK(redirectedAsset(path, true, AssetSource::SdCard) == &asset);
        CHECK(redirectedAsset(path, false, AssetSource::SdCard) == nullptr);
        for (const auto source : {AssetSource::Unchecked, AssetSource::Content, AssetSource::Unavailable}) {
            CHECK(redirectedAsset(path, true, source) == nullptr);
        }
        CHECK(redirectedAsset(path + ".bak", true, AssetSource::SdCard) == nullptr);
        CHECK(redirectedAsset(std::string(asset.path), true, AssetSource::SdCard) == nullptr);
        CHECK(kSdAssetRoot.size() + asset.path.size() < 128);
        bytes += asset.bytes;
    }
    CHECK(bytes == 4877510);
}

TEST_CASE("Save paths and traversal never enter Survey asset routing") {
    for (const auto path : {"", "save:/Lib/sead/nvn_font/nvn_font.ntx",
                           "content:/../Lib/sead/nvn_font/nvn_font.ntx",
                           "content:/Lib/sead/nvn_font/nvn_font.ntx/child",
                           "content://Lib/sead/nvn_font/nvn_font.ntx",
                           "content:/Pack/Actor/Player.pack.zs"}) {
        CHECK(redirectedAsset(path, true, AssetSource::SdCard) == nullptr);
    }
}

TEST_CASE("Replacement call respects aligned signed ARM64 branch limits") {
    CHECK(canBranchLink(-0x08000000));
    CHECK(canBranchLink(0x07fffffc));
    CHECK(canBranchLink(0));
    CHECK_FALSE(canBranchLink(-0x08000004));
    CHECK_FALSE(canBranchLink(0x08000000));
    CHECK_FALSE(canBranchLink(1));
    CHECK_FALSE(canBranchLink(-1));
}
