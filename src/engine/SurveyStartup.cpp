// SPDX-License-Identifier: GPL-2.0-only
#include "SurveyStartup.hpp"

#include <lib.hpp>
#include <nn/fs.h>
#include <atomic>
#include <cstring>

#include "SurveyAssets.hpp"

namespace zonai_survey::engine {
namespace {

using pure::AssetSource;
std::atomic<AssetSource> g_assetSource{AssetSource::Unchecked};
constexpr std::uintptr_t kOpenFileCall = 0x02A26EF8;

struct HookFingerprint {
    std::uintptr_t offset;
    std::uint32_t words[2];
};

constexpr HookFingerprint kHookFingerprints[] = {
    {0x007F61D0, {0xAA1F03F9, 0x92401D1B}},
    {0x00A9123C, {0x940005CA, 0xB001E4A8}},
    {0x00818340, {0x6A28013F, 0x54001160}},
    {0x0081911C, {0xFC1B0FEA, 0x6D00A3E9}},
    {0x02AEFE74, {0xD10143FF, 0xA9027BFD}},
    {0x00858590, {0xD10743FF, 0xA9187BFD}},
    {0x02A267BC, {0xD102C3FF, 0xA9057BFD}},
    {kOpenFileCall, {0x9403C27A, 0xB9005E60}},
};

void assetPath(char (&output)[128], std::string_view root, const pure::SurveyAsset& asset) {
    const auto bytes = root.size() + asset.path.size();
    std::memcpy(output, root.data(), root.size());
    std::memcpy(output + root.size(), asset.path.data(), asset.path.size());
    output[bytes] = '\0';
}

static_assert([] {
    for (const auto& asset : pure::kSurveyAssets) {
        if (pure::kSdAssetRoot.size() + asset.path.size() >= 128) return false;
    }
    return true;
}());

bool completeAssetSet(std::string_view root) {
    for (const auto& asset : pure::kSurveyAssets) {
        char path[128];
        assetPath(path, root, asset);
        nn::fs::FileHandle handle{};
        auto result = nn::fs::OpenFile(&handle, path, nn::fs::OpenMode_Read);
        if (result.IsFailure()) {
            Logging.Log("[survey-startup] asset open failed path=%s result=%08x", path,
                        result.GetInnerValueForDebug());
            return false;
        }
        s64 bytes = 0;
        result = nn::fs::GetFileSize(&bytes, handle);
        nn::fs::CloseFile(handle);
        if (result.IsFailure() || bytes != asset.bytes) {
            Logging.Log("[survey-startup] asset size refused path=%s result=%08x bytes=%lld expected=%u",
                        path, result.GetInnerValueForDebug(), static_cast<long long>(bytes), asset.bytes);
            return false;
        }
    }
    return true;
}

nn::Result openSurveyAsset(nn::fs::FileHandle* handle, const char* path, int mode) {
    const auto source = g_assetSource.load(std::memory_order_acquire);
    if (source != AssetSource::SdCard || mode != nn::fs::OpenMode_Read || !path) {
        return nn::fs::OpenFile(handle, path, mode);
    }
    const auto* asset = pure::redirectedAsset(path, true, source);
    if (!asset) return nn::fs::OpenFile(handle, path, mode);
    char redirected[128];
    assetPath(redirected, pure::kSdAssetRoot, *asset);
    const auto result = nn::fs::OpenFile(handle, redirected, mode);
    if (result.IsFailure()) {
        Logging.Log("[survey-startup] selected asset failed path=%s result=%08x",
                    redirected, result.GetInnerValueForDebug());
    }
    return result;
}

}

bool startupImageSupported(std::uintptr_t mainBase) {
    const auto& text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto& site : kHookFingerprints) {
        if (mainBase + site.offset < text.m_Start ||
            mainBase + site.offset + sizeof(site.words) > text.GetEnd()) {
            Logging.Log("[survey-startup] hook outside text offset=%lx", site.offset);
            return false;
        }
        const auto* words = reinterpret_cast<const std::uint32_t*>(mainBase + site.offset);
        for (unsigned i = 0; i < 2; ++i) {
            if (words[i] != site.words[i]) {
                Logging.Log("[survey-startup] unsupported hook offset=%lx actual=%08x expected=%08x",
                            site.offset + i * 4, words[i], site.words[i]);
                return false;
            }
        }
    }
    const auto distance = reinterpret_cast<std::intptr_t>(&openSurveyAsset) -
                          static_cast<std::intptr_t>(mainBase + kOpenFileCall);
    if (!pure::canBranchLink(distance)) {
        Logging.Log("[survey-startup] asset adapter out of branch range distance=%ld", distance);
        return false;
    }
    return true;
}

void installAssetRedirect() {
    exl::patch::CodePatcher patch(kOpenFileCall);
    patch.BranchLinkInst(reinterpret_cast<void*>(&openSurveyAsset));
}

bool prepareSurveyAssets(sead::Heap* heap) {
    if (!heap) {
        Logging.Log("[survey-startup] graphics refused: heap unavailable");
        return false;
    }
    bool contentComplete = completeAssetSet(pure::kContentAssetRoot);
    bool sdComplete = false;
    if (!contentComplete) {
        const auto result = nn::fs::MountSdCard("survey");
        if (result.IsSuccess()) {
            sdComplete = completeAssetSet(pure::kSdAssetRoot);
        } else {
            Logging.Log("[survey-startup] SD mount failed result=%08x", result.GetInnerValueForDebug());
        }
    }
    const auto source = pure::chooseAssetSource(contentComplete, sdComplete);
    g_assetSource.store(source, std::memory_order_release);
    Logging.Log("[survey-startup] asset source=%s", source == AssetSource::Content ? "content" :
                source == AssetSource::SdCard ? "sd-card" : "unavailable; Survey disabled");
    return source != AssetSource::Unavailable;
}

}
