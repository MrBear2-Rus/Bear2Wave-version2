#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
struct TraceBlackoutStore;
#else
typedef struct TraceBlackoutStore TraceBlackoutStore;
#endif

#ifdef __cplusplus
extern "C" {
#endif

TraceBlackoutStore* trace_blackout_create(void);
void trace_blackout_free(TraceBlackoutStore* store);

void trace_blackout_on_dumpoff(TraceBlackoutStore* store, uint64_t timestamp);
void trace_blackout_on_dumpon(TraceBlackoutStore* store, uint64_t timestamp);

size_t trace_blackout_span_count(const TraceBlackoutStore* store);
int trace_blackout_span_at(const TraceBlackoutStore* store, size_t index, uint64_t* t0, uint64_t* t1);
int trace_blackout_covers(const TraceBlackoutStore* store, uint64_t timestamp);

/** Serialize / restore blackout spans for sidecar cache. */
size_t trace_blackout_export_spans(
    const TraceBlackoutStore* store,
    uint64_t* out_t0,
    uint64_t* out_t1,
    size_t cap,
    int* out_open,
    uint64_t* out_open_start);

void trace_blackout_import_spans(
    TraceBlackoutStore* store,
    const uint64_t* t0,
    const uint64_t* t1,
    size_t count,
    int open,
    uint64_t open_start);

#ifdef __cplusplus
}
#endif
