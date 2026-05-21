#pragma once

#include <cstdlib>
#include <algorithm>
#include <thread>

namespace WaveformPerf {

inline double EnvDouble(const char* name, double defaultVal)
{
    const char* e = std::getenv(name);
    if (!e || !e[0])
        return defaultVal;
    return std::atof(e);
}

inline int EnvInt(const char* name, int defaultVal)
{
    const char* e = std::getenv(name);
    if (!e || !e[0])
        return defaultVal;
    return std::atoi(e);
}

/** Viewport padding when loading trace windows (default 0.2). */
inline double TraceLoadMarginRatio()
{
    return std::max(0.0, std::min(1.0, EnvDouble("BEAR2WAVE_LOAD_MARGIN", 0.2)));
}

inline int CacheDebounceMs() { return std::max(10, EnvInt("BEAR2WAVE_CACHE_DEBOUNCE_MS", 75)); }

inline int TraceLoadDebounceMs() { return std::max(50, EnvInt("BEAR2WAVE_TRACE_LOAD_DEBOUNCE_MS", 120)); }

inline int MaxDrawSegments() { return std::max(500, EnvInt("BEAR2WAVE_MAX_SEGMENTS", 5000)); }

inline unsigned CacheBuildThreads()
{
    const int t = EnvInt("BEAR2WAVE_CACHE_THREADS", 0);
    if (t > 0)
        return static_cast<unsigned>(std::min(t, 16));
    const unsigned hw = std::thread::hardware_concurrency();
    return std::max(1u, std::min(8u, hw > 0 ? hw : 4u));
}

/** VCD file size (MB) above which open shows a convert-to-FST hint. 0 disables. */
inline int VcdWarnThresholdMb() { return std::max(0, EnvInt("BEAR2WAVE_VCD_WARN_MB", 50)); }

/** Max rows inserted into module signal list (avoids UI freeze). */
inline int MaxSignalListRows() { return std::max(500, EnvInt("BEAR2WAVE_MAX_SIGNAL_LIST", 8000)); }

/** AI context: max value-change samples per signal. */
inline int AiMaxEdgesPerSignal() { return std::max(20, EnvInt("BEAR2WAVE_AI_MAX_EDGES_PER_SIG", 200)); }

/** AI context: total UTF-8 character cap. */
inline int AiMaxContextChars() { return std::max(2000, EnvInt("BEAR2WAVE_AI_MAX_CONTEXT_CHARS", 30000)); }

} // namespace WaveformPerf
