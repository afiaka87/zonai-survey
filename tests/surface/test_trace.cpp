// SPDX-License-Identifier: GPL-2.0-only
#include <doctest.h>
#include <type_traits>
#include "../../src/surface/FidelityTrace.cpp"
namespace st = survey_fidelity::trace;
namespace {
void resetTrace() {
    trace_test::io = {};
    st::g_file = {}; st::g_ready = false; st::g_attempted = false;
    st::g_offset = 0; st::g_sequence = 0;
}
}
static_assert(std::is_same_v<decltype(st::begin()), void>);
static_assert(std::is_same_v<decltype(st::record("test")), void>);
TEST_CASE("actual trace writer stops once at capacity without overflowing or retry storms") {
    resetTrace(); st::begin();
    for (unsigned i = 0; i < 10000; ++i) st::record("simulated scan", 5, i);
    CHECK_FALSE(trace_test::io.overflow);
    CHECK(trace_test::io.end <= survey_fidelity::kTraceCapacity);
    CHECK(trace_test::io.stops == 1); CHECK(trace_test::io.closes == 1);
    const auto writes = trace_test::io.writes;
    for (unsigned i = 0; i < 10000; ++i) { st::begin(); st::record("later scan", 5, i); }
    CHECK(trace_test::io.writes == writes); CHECK(trace_test::io.mounts == 1);
    CHECK(trace_test::io.stops == 1);
}
TEST_CASE("actual trace mount create open write and flush failures are one-shot optional diagnostics") {
    using trace_test::Failure;
    for (auto failure : {Failure::Mount, Failure::Create, Failure::Open, Failure::Write, Failure::Flush}) {
        resetTrace(); trace_test::io.failure = failure;
        st::begin();
        const auto writes = trace_test::io.writes;
        for (unsigned i = 0; i < 1000; ++i) { st::begin(); st::record("scan still running"); }
        CHECK(trace_test::io.mounts == 1); CHECK(trace_test::io.stops == 1);
        CHECK(trace_test::io.writes == writes); CHECK_FALSE(trace_test::io.overflow);
        CHECK(trace_test::io.creates <= 16);
    }
}
