#include "core/trace_sidecar_idx.h"

#include "core/waveform_perf.h"

#include <fstapi.h>

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace TraceSidecar {

static constexpr char kMagic[8] = { 'B', 'W', 'I', 'D', 'X', '1', '\0', '\0' };
static constexpr uint32_t kVersion = 1;

bool IdxCacheEnabled()
{
    return WaveformPerf::IdxCacheEnabled() != 0;
}

std::string SidecarPathForTrace(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return {};
    return std::string(utf8_path) + ".bwidx";
}

FileIdentity QueryFileIdentity(const char* utf8_path)
{
    FileIdentity id;
    if (!utf8_path || !utf8_path[0])
        return id;

#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(utf8_path, GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER li;
        li.LowPart = fad.nFileSizeLow;
        li.HighPart = fad.nFileSizeHigh;
        id.size_bytes = li.QuadPart;
        ULARGE_INTEGER mt;
        mt.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        mt.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        id.mtime_unix = mt.QuadPart / 10000000ULL - 11644473600ULL;
    }
#else
    FILE* f = fopen(utf8_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        id.size_bytes = (uint64_t)ftell(f);
        fclose(f);
    }
#endif
    return id;
}

bool SaveFstBlocks(const char* utf8_path, const FileIdentity& id, const std::vector<VcBlock>& blocks)
{
    if (!IdxCacheEnabled() || !utf8_path)
        return false;

    const std::string path = SidecarPathForTrace(utf8_path);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return false;

    fwrite(kMagic, 1, 8, f);
    const uint32_t ver = kVersion;
    fwrite(&ver, sizeof(ver), 1, f);
    fwrite(&id.size_bytes, sizeof(id.size_bytes), 1, f);
    fwrite(&id.mtime_unix, sizeof(id.mtime_unix), 1, f);
    const uint32_t n = (uint32_t)blocks.size();
    fwrite(&n, sizeof(n), 1, f);
    for (const VcBlock& b : blocks) {
        fwrite(&b.beg_tim, sizeof(b.beg_tim), 1, f);
        fwrite(&b.end_tim, sizeof(b.end_tim), 1, f);
        fwrite(&b.blkpos, sizeof(b.blkpos), 1, f);
        fwrite(&b.seclen, sizeof(b.seclen), 1, f);
    }
    fclose(f);
    return true;
}

bool LoadFstBlocks(const char* utf8_path, FileIdentity* out_id, std::vector<VcBlock>* out_blocks)
{
    if (!IdxCacheEnabled() || !utf8_path || !out_blocks)
        return false;

    const std::string path = SidecarPathForTrace(utf8_path);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;

    char magic[8] = {};
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kMagic, 8) != 0) {
        fclose(f);
        return false;
    }

    uint32_t ver = 0;
    FileIdentity id;
    uint32_t n = 0;
    if (fread(&ver, sizeof(ver), 1, f) != 1 || ver != kVersion) {
        fclose(f);
        return false;
    }
    if (fread(&id.size_bytes, sizeof(id.size_bytes), 1, f) != 1 ||
        fread(&id.mtime_unix, sizeof(id.mtime_unix), 1, f) != 1 ||
        fread(&n, sizeof(n), 1, f) != 1) {
        fclose(f);
        return false;
    }

    std::vector<VcBlock> blocks;
    blocks.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VcBlock& b = blocks[i];
        if (fread(&b.beg_tim, sizeof(b.beg_tim), 1, f) != 1 ||
            fread(&b.end_tim, sizeof(b.end_tim), 1, f) != 1 ||
            fread(&b.blkpos, sizeof(b.blkpos), 1, f) != 1 ||
            fread(&b.seclen, sizeof(b.seclen), 1, f) != 1) {
            fclose(f);
            return false;
        }
    }
    fclose(f);

    const FileIdentity cur = QueryFileIdentity(utf8_path);
    if (cur.size_bytes != id.size_bytes || cur.mtime_unix != id.mtime_unix)
        return false;

    if (out_id)
        *out_id = id;
    *out_blocks = std::move(blocks);
    return true;
}

void FindOverlappingBlocks(
    const std::vector<VcBlock>& blocks,
    uint64_t t0,
    uint64_t t1,
    uint32_t* out_first,
    uint32_t* out_last)
{
    if (!out_first || !out_last || blocks.empty()) {
        if (out_first)
            *out_first = 0;
        if (out_last)
            *out_last = 0;
        return;
    }

    uint32_t first = UINT32_MAX;
    uint32_t last = 0;
    bool any = false;

    for (uint32_t i = 0; i < (uint32_t)blocks.size(); ++i) {
        const VcBlock& b = blocks[i];
        if (b.end_tim < t0 || b.beg_tim > t1)
            continue;
        any = true;
        if (i < first)
            first = i;
        if (i > last)
            last = i;
    }

    if (!any) {
        *out_first = 0;
        *out_last = 0;
        return;
    }
    *out_first = first;
    *out_last = last;
}

std::vector<VcBlock> CollectBlocksFromFstReader(fstReaderContext* ctx)
{
    std::vector<VcBlock> out;
    const uint32_t n = fstReaderGetVcBlockCount(ctx);
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const FstVcBlockRec* r = fstReaderGetVcBlock(ctx, i);
        if (!r)
            continue;
        VcBlock b;
        b.beg_tim = r->beg_tim;
        b.end_tim = r->end_tim;
        b.blkpos = (uint64_t)r->blkpos;
        b.seclen = r->seclen;
        out.push_back(b);
    }
    return out;
}

} // namespace TraceSidecar
