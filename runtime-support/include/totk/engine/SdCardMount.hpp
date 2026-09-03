#pragma once

namespace totk::engine {

enum class SdCardMountResult {
    MountedNow,
    AlreadyReady,
    Failed,
};

[[nodiscard]] SdCardMountResult ensureSdCardMounted();

}  
