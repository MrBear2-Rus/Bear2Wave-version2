#include "core/vcd_sidecar_idx.h"

#include "core/trace_sidecar_idx.h"
#include "core/waveform_perf.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace VcdSidecar {

static constexpr char kMagic[8] = { 'B', 'W', 'V', 'C', 'I', 'D', 'X', '1' };
static constexpr uint32_t kVersion = 1;

bool CacheEnabled()
{
    if (WaveformPerf::EnvInt("BEAR2WAVE_VCD_IDX_CACHE", -1) != 0)
        return true;
    return TraceSidecar::IdxCacheEnabled();
}

std::string PathForVcd(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return {};
    return std::string(utf8_path) + ".bwvcdidx";
}

std::string PathForVcdIdxAlias(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return {};
    return std::string(utf8_path) + ".idx";
}

static bool load_time_index_file(
    const char* sidecar_path,
    const char* utf8_path,
    uint64_t data_section_offset,
    std::vector<std::pair<uint64_t, uint64_t>>& out_index)
{
    out_index.clear();
    if (!sidecar_path || !sidecar_path[0])
        return false;

    FILE* f = fopen(sidecar_path, "rb");
    if (!f)
        return false;

    char magic[8] = {};
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kMagic, 8) != 0) {
        fclose(f);
        return false;
    }

    uint32_t ver = 0;
    TraceSidecar::FileIdentity id{};
    uint64_t stored_data_offset = 0;
    uint32_t n = 0;
    if (fread(&ver, sizeof(ver), 1, f) != 1 || ver != kVersion
        || fread(&id.size_bytes, sizeof(id.size_bytes), 1, f) != 1
        || fread(&id.mtime_unix, sizeof(id.mtime_unix), 1, f) != 1
        || fread(&stored_data_offset, sizeof(stored_data_offset), 1, f) != 1
        || fread(&n, sizeof(n), 1, f) != 1) {
        fclose(f);
        return false;
    }

    const TraceSidecar::FileIdentity cur = TraceSidecar::QueryFileIdentity(utf8_path);
    if (cur.size_bytes != id.size_bytes || cur.mtime_unix != id.mtime_unix
        || stored_data_offset != data_section_offset || n == 0) {
        fclose(f);
        return false;
    }

    out_index.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t ts = 0, off = 0;
        if (fread(&ts, sizeof(ts), 1, f) != 1 || fread(&off, sizeof(off), 1, f) != 1) {
            out_index.clear();
            fclose(f);
            return false;
        }
        out_index[i] = {ts, off};
    }

    fclose(f);
    return !out_index.empty();
}

static bool save_time_index_file(
    const char* sidecar_path,
    const TraceSidecar::FileIdentity& id,
    uint64_t data_section_offset,
    const std::vector<std::pair<uint64_t, uint64_t>>& index)
{
    if (!sidecar_path || !sidecar_path[0] || index.empty())
        return false;

    FILE* f = fopen(sidecar_path, "wb");
    if (!f)
        return false;

    fwrite(kMagic, 1, 8, f);
    const uint32_t ver = kVersion;
    fwrite(&ver, sizeof(ver), 1, f);
    fwrite(&id.size_bytes, sizeof(id.size_bytes), 1, f);
    fwrite(&id.mtime_unix, sizeof(id.mtime_unix), 1, f);
    fwrite(&data_section_offset, sizeof(data_section_offset), 1, f);
    const uint32_t n = (uint32_t)index.size();
    fwrite(&n, sizeof(n), 1, f);
    for (const auto& e : index) {
        fwrite(&e.first, sizeof(e.first), 1, f);
        fwrite(&e.second, sizeof(e.second), 1, f);
    }
    fclose(f);
    return true;
}

bool LoadTimeIndex(
    const char* utf8_path,
    uint64_t data_section_offset,
    std::vector<std::pair<uint64_t, uint64_t>>& out_index)
{
    if (!CacheEnabled() || !utf8_path)
        return false;

    const std::string primary = PathForVcd(utf8_path);
    if (load_time_index_file(primary.c_str(), utf8_path, data_section_offset, out_index))
        return true;

    const std::string alias = PathForVcdIdxAlias(utf8_path);
    return load_time_index_file(alias.c_str(), utf8_path, data_section_offset, out_index);
}

bool SaveTimeIndex(
    const char* utf8_path,
    uint64_t data_section_offset,
    const std::vector<std::pair<uint64_t, uint64_t>>& index)
{
    if (!CacheEnabled() || !utf8_path || index.empty())
        return false;

    const TraceSidecar::FileIdentity id = TraceSidecar::QueryFileIdentity(utf8_path);
    const std::string primary = PathForVcd(utf8_path);
    const std::string alias = PathForVcdIdxAlias(utf8_path);
    if (!save_time_index_file(primary.c_str(), id, data_section_offset, index))
        return false;
    (void)save_time_index_file(alias.c_str(), id, data_section_offset, index);
    return true;
}

} // namespace VcdSidecar
