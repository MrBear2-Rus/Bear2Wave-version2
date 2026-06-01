#pragma once

#include <cstdint>

#ifdef __cplusplus

struct TraceLoadProgress {
    uint32_t block_current = 0;
    uint32_t block_total = 0;
    uint32_t signal_count = 0;
    uint64_t elapsed_ms = 0;
};

using TraceLoadProgressFn = void (*)(void* user, const TraceLoadProgress& info);

#endif
