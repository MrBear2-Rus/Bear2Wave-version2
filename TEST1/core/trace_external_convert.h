#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <string>

enum class TraceExternalKind {
    None = 0,
    Vpd,
    Wlf,
    Fsdb,
    Shm,
    Aet
};

struct TraceExternalConfig {
    std::string vpd2vcd_path;
    std::string wlf2vcd_path;
    std::string fsdb2vcd_path;
    std::string shm2vcd_path;
    std::string aet2vcd_path;
    std::string translate_proc_path;
    std::string transaction_proc_path;
    int filter_process_timeout_ms = 500;
    std::string cache_dir;
    int cache_enabled = 1;
};

/** Load config from external_tools.cfg + env (env overrides file). */
TraceExternalConfig trace_external_load_config();

/** Persist config to external_tools.cfg (GUI settings). */
int trace_external_save_config(const TraceExternalConfig& cfg);

/** Default config file path (UTF-8). */
std::string trace_external_config_file_path();

TraceExternalKind trace_external_kind_for_extension(const char* ext);

/** Non-zero when extension is opened via external converter (vpd/wlf/fsdb). */
int trace_external_extension_needs_converter(const char* ext);

/** Resolve converter executable (config → EDA install dirs → PATH). Empty if not found. */
std::string trace_external_probe_tool(TraceExternalKind kind, const TraceExternalConfig* cfg = nullptr);

/** Fill empty vpd2vcd/wlf2vcd/fsdb2vcd/shm2vcd/aet2vcd fields with probed paths (does not save). */
void trace_external_apply_probed_paths(TraceExternalConfig* cfg);

/**
 * Convert source trace to cached VCD; writes output path to out_vcd_path.
 * Returns 0 on success; -1 on error; -2 when converter executable not configured/found.
 */
int trace_external_convert_to_vcd(
    const char* source_path,
    const TraceExternalConfig* cfg,
    char* out_vcd_path,
    size_t out_vcd_path_len,
    char* err_buf,
    size_t err_buf_len);

/** Remove all cached conversions (for tests). */
void trace_external_clear_cache(const TraceExternalConfig* cfg);

extern "C" {
#endif

#ifdef __cplusplus
}
#endif
