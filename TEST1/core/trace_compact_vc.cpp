#include "core/trace_vc.h"

#include "core/ghw_state.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

static const signal_t* trace_vc_resolve(const signal_t* sig)
{
    if (!sig)
        return nullptr;
    if (sig->trace_alias_source)
        return sig->trace_alias_source;
    return sig;
}

static signal_t* trace_vc_resolve_mut(signal_t* sig)
{
    if (!sig)
        return nullptr;
    if (sig->trace_alias_source)
        return sig->trace_alias_source;
    return sig;
}

static uint8_t char_to_v4state(char c)
{
    const char w = bear2wave_nine_state_waveform_char(c);
    switch (w) {
    case '0':
        return 0;
    case '1':
        return 1;
    case 'x':
    case 'X':
        return 2;
    case 'z':
    case 'Z':
        return 3;
    default:
        return 0;
    }
}

static void v4state_to_chars(uint8_t st, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return;
    const char c = (st == 1) ? '1' : (st == 2) ? 'x' : (st == 3) ? 'z' : '0';
    buf[0] = c;
    buf[1] = '\0';
}

int trace_vc_compact_enabled(void)
{
    return WaveformPerf::EnvInt("BEAR2WAVE_COMPACT_VC", 1) != 0 ? 1 : 0;
}

int trace_signal_compact_eligible(const signal_t* sig)
{
    if (!sig || !trace_vc_compact_enabled())
        return 0;
    if (sig->size != 1)
        return 0;
    return 1;
}

size_t trace_vc_count(const signal_t* sig)
{
    sig = trace_vc_resolve(sig);
    return sig ? sig->changes_count : 0;
}

uint64_t trace_vc_timestamp(const signal_t* sig, size_t index)
{
    sig = trace_vc_resolve(sig);
    if (!sig || index >= sig->changes_count)
        return 0;
    if (trace_vc_is_compact(sig))
        return sig->compact_changes[index].timestamp;
    return sig->value_changes[index].timestamp;
}

void trace_vc_format_value(const signal_t* sig, size_t index, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return;
    buf[0] = '\0';
    sig = trace_vc_resolve(sig);
    if (!sig || index >= sig->changes_count)
        return;
    if (trace_vc_is_compact(sig)) {
        v4state_to_chars(sig->compact_changes[index].state, buf, buf_len);
        return;
    }
    bear2wave_nine_state_display_label(sig->value_changes[index].value, buf, buf_len);
}

const value_change_t* trace_vc_legacy_ptr(const signal_t* sig)
{
    if (!sig || trace_vc_is_compact(sig))
        return nullptr;
    return sig->value_changes;
}

static int append_full(signal_t* sig, timestamp_t ts, const char* value)
{
    if (sig->changes_count >= sig->changes_capacity) {
        size_t ncap = sig->changes_capacity ? (sig->changes_capacity * 2u)
                                            : (size_t)VCD_VALUE_CHANGES_INITIAL_CAP;
        value_change_t* nv = (value_change_t*)realloc(sig->value_changes, ncap * sizeof(value_change_t));
        if (!nv)
            return -1;
        sig->value_changes = nv;
        sig->changes_capacity = ncap;
    }
    value_change_t* ch = &sig->value_changes[sig->changes_count++];
    ch->timestamp = ts;
    strncpy(ch->value, value, VCD_SIGNAL_SIZE - 1);
    ch->value[VCD_SIGNAL_SIZE - 1] = '\0';
    return 0;
}

static int append_compact(signal_t* sig, timestamp_t ts, const char* value)
{
    if (sig->changes_count >= sig->changes_capacity) {
        size_t ncap = sig->changes_capacity ? (sig->changes_capacity * 2u)
                                            : (size_t)VCD_VALUE_CHANGES_INITIAL_CAP;
        trace_compact_change_t* nv =
            (trace_compact_change_t*)realloc(sig->compact_changes, ncap * sizeof(trace_compact_change_t));
        if (!nv)
            return -1;
        sig->compact_changes = nv;
        sig->changes_capacity = ncap;
    }
    trace_compact_change_t* ch = &sig->compact_changes[sig->changes_count++];
    ch->timestamp = static_cast<uint64_t>(ts);
    ch->state = char_to_v4state(value && value[0] ? value[0] : '0');
    return 0;
}

int trace_vc_append(signal_t* sig, timestamp_t ts, const char* value)
{
    if (!sig || !value)
        return -1;
    sig = trace_vc_resolve_mut(sig);
    if (!sig)
        return -1;

    if (!trace_signal_compact_eligible(sig)) {
        if (trace_vc_is_compact(sig)) {
            /* Should not happen for wide signals; keep compact if already started. */
        } else {
            sig->vc_storage = TRACE_VC_STORAGE_FULL;
        }
    }

    if (trace_signal_compact_eligible(sig) && sig->changes_count == 0 && !trace_vc_is_compact(sig)) {
        sig->vc_storage = TRACE_VC_STORAGE_COMPACT;
        sig->value_changes = nullptr;
        sig->compact_changes = nullptr;
        sig->changes_capacity = 0;
    }

    sig->changes_sorted = 0;
    if (trace_vc_is_compact(sig))
        return append_compact(sig, ts, value);
    return append_full(sig, ts, value);
}

void trace_vc_free_storage(signal_t* sig)
{
    if (!sig)
        return;
    free(sig->value_changes);
    free(sig->compact_changes);
    sig->value_changes = nullptr;
    sig->compact_changes = nullptr;
    sig->changes_count = 0;
    sig->changes_capacity = 0;
    sig->vc_storage = TRACE_VC_STORAGE_FULL;
    sig->changes_sorted = 0;
}

void trace_vc_shrink_to_fit(signal_t* sig)
{
    if (!sig || sig->changes_count == 0) {
        trace_vc_free_storage(sig);
        return;
    }
    if (sig->changes_capacity <= sig->changes_count)
        return;
    if (trace_vc_is_compact(sig)) {
        trace_compact_change_t* nv = (trace_compact_change_t*)realloc(
            sig->compact_changes, sig->changes_count * sizeof(trace_compact_change_t));
        if (nv) {
            sig->compact_changes = nv;
            sig->changes_capacity = sig->changes_count;
        }
        return;
    }
    value_change_t* nv = (value_change_t*)realloc(
        sig->value_changes, sig->changes_count * sizeof(value_change_t));
    if (nv) {
        sig->value_changes = nv;
        sig->changes_capacity = sig->changes_count;
    }
}

void trace_vc_sort(signal_t* sig)
{
    if (!sig || sig->changes_count < 2 || sig->changes_sorted)
        return;
    if (trace_vc_is_compact(sig)) {
        std::stable_sort(
            sig->compact_changes,
            sig->compact_changes + sig->changes_count,
            [](const trace_compact_change_t& a, const trace_compact_change_t& b) {
                return a.timestamp < b.timestamp;
            });
    } else if (sig->value_changes) {
        std::stable_sort(
            sig->value_changes,
            sig->value_changes + sig->changes_count,
            [](const value_change_t& a, const value_change_t& b) { return a.timestamp < b.timestamp; });
    }
    sig->changes_sorted = 1;
}
