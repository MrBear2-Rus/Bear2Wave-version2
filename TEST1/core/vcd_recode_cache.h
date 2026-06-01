#pragma once

#include "core/trace_sidecar_idx.h"
#include "vcd.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace VcdRecodeCache {

struct BlackoutSnapshot {
    std::vector<std::pair<uint64_t, uint64_t>> spans;
    bool open = false;
    uint64_t open_start = 0;
};

/** Loaded metadata from `<vcd>.bwvc` (block directory + signal table). */
struct Meta {
    std::vector<std::string> signal_ids;
    std::vector<uint16_t> signal_widths;
    std::vector<TraceSidecar::VcBlock> blocks;
    uint64_t max_timestamp = 0;
    BlackoutSnapshot blackout;
    std::string cache_path;
};

/** Default on; set BEAR2WAVE_VCD_RECODE=0 to disable. */
bool Enabled();

/** Sidecar path: `<vcdpath>.bwvc` */
std::string PathForVcd(const char* utf8_path);

/** Load sidecar if present and source identity matches. */
bool LoadMeta(const char* utf8_path, uint64_t data_section_offset, Meta* out);

/** One-pass encode of VCD data section → `.bwvc`; also fills blackout on vcd. */
int BuildAndSave(
    const char* utf8_path,
    FILE* file,
    uint64_t file_size,
    uint64_t data_section_offset,
    vcd_t* vcd,
    const std::unordered_map<std::string, signal_t*>& id_to_sig,
    std::atomic<bool>* cancel,
    void (*progress_fn)(void*, uint32_t, uint32_t, uint32_t, uint64_t),
    void* progress_user);

/** Apply stored blackout spans to vcd->trace_blackout. */
void ApplyBlackout(vcd_t* vcd, const Meta& meta);

/** Decode overlapping blocks into selected signals for [t0,t1]. Returns 0/-1/1. */
int DecodeWindow(
    const Meta& meta,
    const std::unordered_map<std::string, signal_t*>& id_to_sig,
    const std::unordered_set<std::string>& wanted_ids,
    uint64_t t0,
    uint64_t t1,
    std::atomic<bool>* cancel);

} // namespace VcdRecodeCache
