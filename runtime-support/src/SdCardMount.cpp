#include "totk/engine/SdCardMount.hpp"

#include <nn/fs.h>

namespace nn::fs {
nn::Result MountSdCard(char const* mount);
}  

namespace totk::engine {
namespace {

bool gSdCardReady = false;

}  

SdCardMountResult ensureSdCardMounted() {
    if (gSdCardReady) return SdCardMountResult::AlreadyReady;

    const nn::Result result = nn::fs::MountSdCard("sdcard");
    if (result.IsFailure()) return SdCardMountResult::Failed;

    gSdCardReady = true;
    return SdCardMountResult::MountedNow;
}

}  
