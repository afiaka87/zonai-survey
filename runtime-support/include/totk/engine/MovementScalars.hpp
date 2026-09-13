#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace totk::movement {

enum class Axis : std::uint8_t {
    JumpHeight,
    SprintSpeed,
    GlideSpeed,
    ClimbSpeed,
    ClimbJump,
    Count,
};

inline constexpr std::uint16_t kUnitScaleQ8 = 256;
inline constexpr float kQ8ToFloat = 1.0f / 256.0f;

namespace detail {

inline constexpr std::size_t kAxisCount = static_cast<std::size_t>(Axis::Count);

inline std::atomic<std::uint16_t> g_scaleQ8[kAxisCount] = {
    kUnitScaleQ8,
    kUnitScaleQ8,
    kUnitScaleQ8,
    kUnitScaleQ8,
    kUnitScaleQ8,
};

inline std::atomic<bool> g_available[kAxisCount] = {false, false, false,
                                                    false, false};

static_assert(std::atomic<std::uint16_t>::is_always_lock_free,
              "movement scalars are read from a load-time inline hook");

static_assert(kAxisCount == 5,
              "extend g_scaleQ8 and g_available when adding an Axis");

inline std::size_t index(Axis axis) { return static_cast<std::size_t>(axis); }

}

inline void publishScaleQ8(Axis axis, std::uint16_t scaleQ8) {
    detail::g_scaleQ8[detail::index(axis)].store(scaleQ8,
                                                 std::memory_order_release);
}

[[nodiscard]] inline std::uint16_t scaleQ8(Axis axis) {
    return detail::g_scaleQ8[detail::index(axis)].load(std::memory_order_acquire);
}

[[nodiscard]] inline float scale(Axis axis) {
    return static_cast<float>(scaleQ8(axis)) * kQ8ToFloat;
}

[[nodiscard]] inline bool isVanilla(Axis axis) {
    return scaleQ8(axis) == kUnitScaleQ8;
}

inline void markAvailable(Axis axis, bool available) {
    detail::g_available[detail::index(axis)].store(available,
                                                   std::memory_order_release);
}

[[nodiscard]] inline bool available(Axis axis) {
    return detail::g_available[detail::index(axis)].load(
        std::memory_order_acquire);
}

}
