// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace survey_fidelity::trace {
#if defined(SURVEY_FIDELITY_PLAYGROUND) && !SURVEY_FIDELITY_PLAYGROUND
inline void begin() {}
inline void record(const char*,unsigned=0,std::uint64_t=0,std::uint64_t=0,std::uint64_t=0,std::uint64_t=0) {}
#else

void begin();
void record(const char* event, unsigned mode = 0, std::uint64_t a = 0,
            std::uint64_t b = 0, std::uint64_t c = 0, std::uint64_t d = 0);
#endif
}
