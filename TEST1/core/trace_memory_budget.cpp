#include "core/trace_memory_budget.h"

#include "core/waveform_perf.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace {

uint64_t g_lru_clock = 0;
std::unordered_map<const signal_t*, uint64_t> g_lru_tick;

uint64_t lru_of(const signal_t* sig)
{
    auto it = g_lru_tick.find(sig);
    return it != g_lru_tick.end() ? it->second : 0;
}

void clear_lru_for_vcd(const vcd_t* vcd)
{
    if (!vcd)
        return;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        g_lru_tick.erase(&n->signal);
}

} // namespace

extern "C" size_t trace_count_loaded_changes(const vcd_t* vcd)
{
    if (!vcd)
        return 0;
    size_t total = 0;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
        if (n->signal.changes_count > 0)
            total += n->signal.changes_count;
    }
    return total;
}

extern "C" void trace_memory_touch(signal_t* sig)
{
    if (!sig)
        return;
    g_lru_tick[sig] = ++g_lru_clock;
}

extern "C" void trace_memory_budget_enforce(vcd_t* vcd, signal_t** protect, size_t protect_count)
{
    const size_t limit = static_cast<size_t>(WaveformPerf::MaxLoadedChanges());
    if (!vcd || limit == 0)
        return;

    std::unordered_set<signal_t*> pinned;
    if (protect) {
        for (size_t i = 0; i < protect_count; ++i) {
            if (protect[i])
                pinned.insert(protect[i]);
        }
    }

    auto over = [&]() { return trace_count_loaded_changes(vcd) > limit; };

    while (over()) {
        signal_t* victim = nullptr;
        uint64_t oldest = UINT64_MAX;
        for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
            signal_t* sig = &n->signal;
            if (sig->changes_count == 0)
                continue;
            if (pinned.count(sig))
                continue;
            const uint64_t t = lru_of(sig);
            if (t < oldest) {
                oldest = t;
                victim = sig;
            }
        }
        if (!victim)
            break;
        vcd_signal_clear_trace_data(victim);
        g_lru_tick.erase(victim);
    }
}

extern "C" void trace_memory_budget_on_vcd_free(vcd_t* vcd)
{
    clear_lru_for_vcd(vcd);
}
