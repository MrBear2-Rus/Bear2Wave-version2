#pragma once

#include "core/trace_external_convert.h"

/** Filter-process settings stored in external_tools.cfg (subset of TraceExternalConfig). */
struct TraceFilterProcessConfig {
    std::string translate_proc_path;
    std::string transaction_proc_path;
    int process_timeout_ms = 500;
};

TraceFilterProcessConfig trace_filter_load_config();
int trace_filter_save_config(const TraceFilterProcessConfig& cfg);

/** Fill empty translate/transaction paths from tools/ probe (does not save). */
void trace_filter_apply_probed_paths(TraceFilterProcessConfig* cfg);

/** Resolve translate_proc executable (config → tools/ → PATH). */
std::string trace_filter_probe_translate_proc(const TraceFilterProcessConfig* cfg = nullptr);

/** Resolve transaction_proc executable (config → tools/ → PATH). */
std::string trace_filter_probe_transaction_proc(const TraceFilterProcessConfig* cfg = nullptr);

inline TraceFilterProcessConfig trace_filter_from_external(const TraceExternalConfig& ext)
{
    TraceFilterProcessConfig cfg;
    cfg.translate_proc_path = ext.translate_proc_path;
    cfg.transaction_proc_path = ext.transaction_proc_path;
    cfg.process_timeout_ms = ext.filter_process_timeout_ms;
    return cfg;
}

inline void trace_filter_to_external(TraceExternalConfig* ext, const TraceFilterProcessConfig& fc)
{
    if (!ext)
        return;
    ext->translate_proc_path = fc.translate_proc_path;
    ext->transaction_proc_path = fc.transaction_proc_path;
    ext->filter_process_timeout_ms = fc.process_timeout_ms;
}
