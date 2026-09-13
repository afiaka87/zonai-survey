// SPDX-License-Identifier: GPL-2.0-only

// Copyright (C) Clay Mullis

#pragma once

#include "totk/core/Result.hpp"
#include "totk/engine/Pointer.hpp"
#include "totk/engine/Totk121Offsets.hpp"

#include <cstdint>

namespace totk::engine {

enum class GameDataError : std::uint8_t {
    None,
    MainImageUnavailable,
    ManagerHolderUnavailable,
    ManagerUnavailable,
    QueueStoreUnavailable,
    QueueUnallocated,
    QueueFull,
    GameplayNotReady,
    EngineFunctionUnavailable,
    FlagUnavailable,
};

struct GameDataQueueRoom {
    std::int32_t capacity = 0;
    std::int32_t writeIndex = 0;
    std::int32_t requestedPushes = 0;
    std::int32_t reservedHeadroom = layout::kGameDataQueueHeadroom;
};

using GameDataManagerResult =
    core::Result<std::uintptr_t, GameDataError>;
using GameDataQueueResult =
    core::Result<GameDataQueueRoom, GameDataError>;

[[nodiscard]] inline GameDataManagerResult
resolveGameDataManagerFromHolder(std::uintptr_t holderSlot) {
    if (!isPlausibleAddress(holderSlot)) {
        return GameDataManagerResult::failure(
            GameDataError::ManagerHolderUnavailable);
    }
    const auto holder = readMemory<std::uintptr_t>(holderSlot);
    if (!isPlausibleAddress(holder)) {
        return GameDataManagerResult::failure(
            GameDataError::ManagerHolderUnavailable);
    }
    const auto manager = readMemory<std::uintptr_t>(holder);
    if (!isPlausibleAddress(manager)) {
        return GameDataManagerResult::failure(GameDataError::ManagerUnavailable);
    }
    return GameDataManagerResult::success(manager);
}

[[nodiscard]] inline GameDataQueueResult
inspectGameDataQueue(std::uintptr_t store, std::int32_t pushes,
                     std::int32_t headroom = layout::kGameDataQueueHeadroom) {
    if (!isPlausibleAddress(store) || pushes < 0 || headroom < 0) {
        return GameDataQueueResult::failure(GameDataError::QueueStoreUnavailable);
    }
    const auto capacity = readMemory<std::int32_t>(
        store + layout::kGameDataQueueCapacity);
    const auto buffer = readMemory<std::uintptr_t>(
        store + layout::kGameDataQueueBuffer);
    const auto writeIndex = static_cast<std::int32_t>(
        readMemory<std::uint32_t>(store + layout::kGameDataQueueControl) &
        layout::kGameDataQueueIndexMask);
    if (capacity <= 0 || !isPlausibleAddress(buffer)) {
        return GameDataQueueResult::failure(GameDataError::QueueUnallocated);
    }

    const GameDataQueueRoom room{capacity, writeIndex, pushes, headroom};
    if (writeIndex > capacity || pushes > capacity - writeIndex - headroom) {
        return GameDataQueueResult{room, GameDataError::QueueFull, false};
    }
    return GameDataQueueResult::success(room);
}

enum class GameDataWriteReadiness : std::uint8_t {
    NotReady,
    LiveGameplayAndWorkerAlive,
};

using GameDataGetIntFunction =
    std::uint32_t (*)(std::uintptr_t, std::int32_t*, std::uint32_t);
using GameDataSetIntFunction =
    std::uint32_t (*)(std::uintptr_t, std::int32_t, std::uint32_t);

struct GameDataFunctions {
    GameDataGetIntFunction getInt = nullptr;
    GameDataSetIntFunction setInt = nullptr;
};

class GameDataService {
public:
    GameDataService(std::uintptr_t managerHolderSlot, GameDataFunctions functions)
        : managerHolderSlot_(managerHolderSlot), functions_(functions) {}

    [[nodiscard]] static GameDataService fromMainBase(std::uintptr_t mainBase) {
        if (!isPlausibleAddress(mainBase)) return GameDataService{0, {}};
        return GameDataService{
            mainBase + Totk121Offsets::kGameDataManagerIndirect.value,
            GameDataFunctions{
                reinterpret_cast<GameDataGetIntFunction>(
                    mainBase + Totk121Offsets::kGameDataGetInt.value),
                reinterpret_cast<GameDataSetIntFunction>(
                    mainBase + Totk121Offsets::kGameDataSetInt.value),
            },
        };
    }

    [[nodiscard]] core::Result<std::int32_t, GameDataError>
    readInt(std::uint32_t nameHash) const {
        const auto manager = resolveGameDataManagerFromHolder(managerHolderSlot_);
        if (!manager) {
            return core::Result<std::int32_t, GameDataError>::failure(manager.error);
        }
        if (!functions_.getInt) {
            return core::Result<std::int32_t, GameDataError>::failure(
                GameDataError::EngineFunctionUnavailable);
        }
        std::int32_t value = 0;
        if ((functions_.getInt(manager.value, &value, nameHash) & 1U) == 0) {
            return core::Result<std::int32_t, GameDataError>::failure(
                GameDataError::FlagUnavailable);
        }
        return core::Result<std::int32_t, GameDataError>::success(value);
    }

    [[nodiscard]] GameDataError writeInt(
        std::uint32_t nameHash, std::int32_t value,
        GameDataWriteReadiness readiness) const {
        if (readiness != GameDataWriteReadiness::LiveGameplayAndWorkerAlive) {
            return GameDataError::GameplayNotReady;
        }
        const auto manager = resolveGameDataManagerFromHolder(managerHolderSlot_);
        if (!manager) return manager.error;
        const auto queue =
            inspectGameDataQueue(manager.value + layout::kGameDataIntStore, 1);
        if (!queue) return queue.error;
        if (!functions_.setInt) return GameDataError::EngineFunctionUnavailable;
        functions_.setInt(manager.value, value, nameHash);
        return GameDataError::None;
    }

private:
    std::uintptr_t managerHolderSlot_ = 0;
    GameDataFunctions functions_{};
};

}
