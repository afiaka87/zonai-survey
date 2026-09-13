#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace totk::guards {

enum class Guard : std::uint8_t {
    PlayerStamina,
    ZonaiBattery,
    Arrows,
    ZonaiDevices,
    HorseStamina,
    ClimbGrip,
    Count,
};

namespace detail {

inline constexpr std::size_t kGuardCount = static_cast<std::size_t>(Guard::Count);

inline std::atomic<bool> g_active[kGuardCount] = {false, false, false,
                                                  false, false, false};

inline std::atomic<bool> g_available[kGuardCount] = {false, false, false,
                                                     false, false, false};

static_assert(std::atomic<bool>::is_always_lock_free,
              "resource guards are read from a load-time inline hook");

static_assert(kGuardCount == 6,
              "extend g_active and g_available when adding a Guard");

inline std::size_t index(Guard guard) { return static_cast<std::size_t>(guard); }

}

inline void publishActive(Guard guard, bool active) {
    detail::g_active[detail::index(guard)].store(active,
                                                 std::memory_order_release);
}

[[nodiscard]] inline bool active(Guard guard) {
    return detail::g_active[detail::index(guard)].load(
        std::memory_order_acquire);
}

inline void markAvailable(Guard guard, bool available) {
    detail::g_available[detail::index(guard)].store(available,
                                                    std::memory_order_release);
}

[[nodiscard]] inline bool available(Guard guard) {
    return detail::g_available[detail::index(guard)].load(
        std::memory_order_acquire);
}

}
