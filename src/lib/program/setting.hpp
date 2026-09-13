#pragma once
#include <common.hpp>
#if SURVEY_FIDELITY_PLAYGROUND
#define EXL_MODULE_NAME "zonai-survey-fidelity-playground"
#else
#define EXL_MODULE_NAME "zonai-survey"
#endif
#define EXL_USE_FAKEHEAP
namespace exl::setting {
constexpr size_t HeapSize = 0x10000;
constexpr size_t JitSize = 0x4000;
constexpr size_t InlinePoolSize = 0x1000;
constexpr size_t LogBufferSize = 512;
static_assert(ALIGN_UP(JitSize, PAGE_SIZE) == JitSize);
static_assert(ALIGN_UP(InlinePoolSize, PAGE_SIZE) == InlinePoolSize);
}
