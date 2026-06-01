#pragma once

#include <fstapi.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace TraceSidecar {

struct VcBlock {
    uint64_t beg_tim = 0;
    uint64_t end_tim = 0;
    uint64_t blkpos = 0;
    uint64_t seclen = 0;
};

struct FileIdentity {
    uint64_t size_bytes = 0;
    uint64_t mtime_unix = 0;
};

/** Default on; set BEAR2WAVE_IDX_CACHE=0 to disable. */
bool IdxCacheEnabled();

std::string SidecarPathForTrace(const char* utf8_path);

/** Write .bwidx next to trace file. */
bool SaveFstBlocks(const char* utf8_path, const FileIdentity& id, const std::vector<VcBlock>& blocks);

/** Load if magic/version/id match; returns false if missing or stale. */
bool LoadFstBlocks(const char* utf8_path, FileIdentity* out_id, std::vector<VcBlock>* out_blocks);

/** Find inclusive [first,last] block indices overlapping [t0,t1]. */
void FindOverlappingBlocks(
    const std::vector<VcBlock>& blocks,
    uint64_t t0,
    uint64_t t1,
    uint32_t* out_first,
    uint32_t* out_last);

FileIdentity QueryFileIdentity(const char* utf8_path);

std::vector<VcBlock> CollectBlocksFromFstReader(fstReaderContext* ctx);

} // namespace TraceSidecar
