#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace VcdSidecar {

bool CacheEnabled();

/** Primary sidecar: `<vcdpath>.bwvcdidx` (Bear2Wave native). */
std::string PathForVcd(const char* utf8_path);

/** GTKWave-style alias: `<vcdpath>.idx` (same payload as `.bwvcdidx`). */
std::string PathForVcdIdxAlias(const char* utf8_path);

/** Load timestamp→file-offset index if sidecar matches source file identity. */
bool LoadTimeIndex(
    const char* utf8_path,
    uint64_t data_section_offset,
    std::vector<std::pair<uint64_t, uint64_t>>& out_index);

/** Write sidecar after building time index in memory (writes `.bwvcdidx` + `.idx`). */
bool SaveTimeIndex(
    const char* utf8_path,
    uint64_t data_section_offset,
    const std::vector<std::pair<uint64_t, uint64_t>>& index);

} // namespace VcdSidecar
