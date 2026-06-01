#include "core/trace_blackout.h"

#include <algorithm>
#include <limits>
#include <vector>

struct TraceBlackoutStore {
    std::vector<std::pair<uint64_t, uint64_t>> spans;
    bool open = false;
    uint64_t open_start = 0;
};

TraceBlackoutStore* trace_blackout_create(void)
{
    return new TraceBlackoutStore();
}

void trace_blackout_free(TraceBlackoutStore* store)
{
    delete store;
}

void trace_blackout_on_dumpoff(TraceBlackoutStore* store, uint64_t timestamp)
{
    if (!store || store->open)
        return;
    store->open = true;
    store->open_start = timestamp;
}

void trace_blackout_on_dumpon(TraceBlackoutStore* store, uint64_t timestamp)
{
    if (!store || !store->open)
        return;
    store->spans.emplace_back(store->open_start, timestamp);
    store->open = false;
}

size_t trace_blackout_span_count(const TraceBlackoutStore* store)
{
    if (!store)
        return 0;
    size_t n = store->spans.size();
    if (store->open)
        ++n;
    return n;
}

int trace_blackout_span_at(const TraceBlackoutStore* store, size_t index, uint64_t* t0, uint64_t* t1)
{
    if (!store || !t0 || !t1)
        return 0;

    if (index < store->spans.size()) {
        *t0 = store->spans[index].first;
        *t1 = store->spans[index].second;
        return 1;
    }

    if (store->open && index == store->spans.size()) {
        *t0 = store->open_start;
        *t1 = std::numeric_limits<uint64_t>::max();
        return 1;
    }

    return 0;
}

int trace_blackout_covers(const TraceBlackoutStore* store, uint64_t timestamp)
{
    if (!store)
        return 0;

    for (const auto& span : store->spans) {
        if (timestamp >= span.first && timestamp <= span.second)
            return 1;
    }
    if (store->open && timestamp >= store->open_start)
        return 1;
    return 0;
}

size_t trace_blackout_export_spans(
    const TraceBlackoutStore* store,
    uint64_t* out_t0,
    uint64_t* out_t1,
    size_t cap,
    int* out_open,
    uint64_t* out_open_start)
{
    if (!store)
        return 0;
    if (out_open)
        *out_open = store->open ? 1 : 0;
    if (out_open_start)
        *out_open_start = store->open_start;
    const size_t n = store->spans.size();
    if (!out_t0 || !out_t1 || cap == 0)
        return n;
    const size_t copy = std::min(n, cap);
    for (size_t i = 0; i < copy; ++i) {
        out_t0[i] = store->spans[i].first;
        out_t1[i] = store->spans[i].second;
    }
    return n;
}

void trace_blackout_import_spans(
    TraceBlackoutStore* store,
    const uint64_t* t0,
    const uint64_t* t1,
    size_t count,
    int open,
    uint64_t open_start)
{
    if (!store)
        return;
    store->spans.clear();
    store->spans.reserve(count);
    for (size_t i = 0; i < count; ++i)
        store->spans.emplace_back(t0[i], t1[i]);
    store->open = open != 0;
    store->open_start = open_start;
}
