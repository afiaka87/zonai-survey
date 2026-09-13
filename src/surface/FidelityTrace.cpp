// SPDX-License-Identifier: GPL-2.0-only
#include "FidelityTrace.hpp"
#include "FidelityPolicy.hpp"
#include <lib.hpp>
#include <nn/fs.h>
#include <atomic>
#include <cstring>

namespace survey_fidelity::trace {
namespace {
nn::fs::FileHandle g_file{};
std::atomic<bool> g_ready{};
bool g_attempted{};
std::uint64_t g_offset{}, g_sequence{};
void fail(const char* step, unsigned result) {
    Logging.Log("[survey-fidelity] TRACE_STOPPED step=%s rc=%08x; scanning continues\n", step, result);
    if (g_ready.exchange(false)) nn::fs::CloseFile(g_file);
}
}
void record(const char* event, unsigned mode, std::uint64_t a, std::uint64_t b,
            std::uint64_t c, std::uint64_t d) {
    if (!g_ready.load()) return;
    char line[256]{};
    nn::util::SNPrintf(line, sizeof(line),
        "%04llu tick=%016llx mode=%u %s a=%016llx b=%016llx c=%016llx d=%016llx\n",
        static_cast<unsigned long long>(g_sequence++), static_cast<unsigned long long>(svcGetSystemTick()),
        mode, event, static_cast<unsigned long long>(a), static_cast<unsigned long long>(b),
        static_cast<unsigned long long>(c), static_cast<unsigned long long>(d));
    const auto bytes = std::strlen(line);
    if (!traceFits(g_offset, bytes)) { fail("capacity", 0); return; }
    auto result = nn::fs::WriteFile(g_file, g_offset, line, bytes, nn::fs::WriteOption{0});
    if (result.IsFailure()) { fail("write", result.GetInnerValueForDebug()); return; }
    result = nn::fs::FlushFile(g_file);
    if (result.IsFailure()) { fail("flush", result.GetInnerValueForDebug()); return; }
    g_offset += bytes;
}
void begin() {
    if (g_attempted) return;
    g_attempted = true;
    auto result = nn::fs::MountSdCard("surveyfidelity");
    if (result.IsFailure()) { fail("mount", result.GetInnerValueForDebug()); return; }
    char path[128]{};
    const auto nonce = static_cast<unsigned long long>(svcGetSystemTick());

    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        nn::util::SNPrintf(path, sizeof(path), "surveyfidelity:/survey-fidelity-sweep11-%016llx-%02u.log", nonce, attempt);
        result = nn::fs::CreateFile(path, kTraceCapacity);
        if (result.IsSuccess()) break;
    }
    if (result.IsFailure()) { fail("create", result.GetInnerValueForDebug()); return; }
    result = nn::fs::OpenFile(&g_file, path, nn::fs::OpenMode_Write);
    if (result.IsFailure()) { fail("open", result.GetInnerValueForDebug()); return; }
    g_ready.store(true);
    Logging.Log("[survey-fidelity] TRACE_READY path=%s bytes=%u\n", path, unsigned(kTraceCapacity));
    record("sweep11 trace-open");
}
}
