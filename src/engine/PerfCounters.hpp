// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Clay Mullis
#pragma once

#include <atomic>
#include <cstdint>

namespace zonai_survey::engine::perf {

#if OVERLAY_DEBUG_HUD

inline std::uint64_t now() { return svcGetSystemTick(); }

inline std::uint64_t frequency() { return 19200000ull; }

struct Bucket {
    std::uint64_t sum = 0;
    std::uint32_t count = 0;
    void add(std::uint64_t delta) { sum += delta; ++count; }
    void reset() { sum = 0; count = 0; }
};

class Timer {
  public:
    explicit Timer(Bucket& bucket) : bucket_(&bucket), start_(now()) {}
    ~Timer() { bucket_->add(now() - start_); }
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

  private:
    Bucket* bucket_;
    std::uint64_t start_;
};

inline Bucket gameTick;     
inline Bucket gameScan;     
inline Bucket gameRebuild;  
inline Bucket gameGlyphs;   
inline Bucket gameStatus;   
inline std::uint32_t gameStatusChanges = 0;  

inline std::atomic<std::uint32_t> workerSeen{0};
inline std::atomic<std::uint64_t> workerSum{0};
inline std::atomic<std::uint32_t> workerCalls{0};
inline void addWorkerSample(std::uint64_t delta) {
    workerSum.fetch_add(delta, std::memory_order_relaxed);
    workerCalls.fetch_add(1, std::memory_order_relaxed);
}

inline Bucket drawSurveyTotal;  
inline Bucket drawVis;          
inline Bucket drawFillOutline;  
inline Bucket drawFillCore;     
inline Bucket drawGlyphText;    
inline std::uint32_t drawLayerCalls = 0;     
inline std::uint32_t drawGameplayCalls = 0;  
inline std::uint32_t drawSkipNoFrame = 0;    
inline std::uint32_t drawSkipHidden = 0;     
inline std::uint32_t drawSkipEmpty = 0;      

#else  

inline std::uint64_t now() { return 0; }
inline std::uint64_t frequency() { return 1; }

struct Bucket {
    void add(std::uint64_t) {}
    void reset() {}
};

class Timer {
  public:
    explicit Timer(Bucket&) {}
};

inline Bucket gameTick, gameScan, gameRebuild, gameGlyphs, gameStatus;
inline std::uint32_t gameStatusChanges = 0;
inline std::atomic<std::uint32_t> workerSeen{0};
inline std::atomic<std::uint64_t> workerSum{0};
inline std::atomic<std::uint32_t> workerCalls{0};
inline void addWorkerSample(std::uint64_t) {}
inline Bucket drawSurveyTotal, drawVis, drawFillOutline, drawFillCore,
    drawGlyphText;
inline std::uint32_t drawLayerCalls = 0, drawGameplayCalls = 0,
    drawSkipNoFrame = 0, drawSkipHidden = 0, drawSkipEmpty = 0;

#endif

}  
