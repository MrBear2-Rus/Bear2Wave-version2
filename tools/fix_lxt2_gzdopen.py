#!/usr/bin/env python3
"""Replace gzdopen(dup(fileno(..))) with inflate in lxt2_read.c for MSVC compat."""

import sys

path = r'third_party\gtkwave\liblxt\lxt2_read.c'

with open(path, 'r', encoding='utf-8', errors='replace') as f:
    content = f.read()

# --- First replacement: facility names decompression ---
old1 = (
    '\t\t\tfprintf(stderr, "[LXT2-DBG] masks allocated, about to gzdopen\\n"); fflush(stderr);\n'
    '\n'
    '\t\t\t{\n'
    '\t\t\tint fd_raw = fileno(lt->handle);\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] fileno=%d\\n", fd_raw); fflush(stderr);\n'
    '\t\t\tint fd_dup = dup(fd_raw);\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] dup=%d\\n", fd_dup); fflush(stderr);\n'
    '\t\t\tlt->zhandle = gzdopen(fd_dup, "rb");\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] zhandle=%p\\n", (void*)lt->zhandle); fflush(stderr);\n'
    '\t\t\t}\n'
    '\t\t\tm=(char *)malloc(lt->zfacname_predec_size);\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] malloc for facnames: m=%p\\n", (void*)m); fflush(stderr);\n'
    '\t\t\trc=gzread(lt->zhandle, m, lt->zfacname_predec_size);\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] gzread facnames: rc=%d (expected %d)\\n", rc, (int)lt->zfacname_predec_size); fflush(stderr);\n'
    '\t\t\tgzclose(lt->zhandle); lt->zhandle=NULL;\n'
    '\n'
    '\t\t\tif(((lxtint32_t)rc)!=lt->zfacname_predec_size)'
)

new1 = (
    '\t\t\t/* Bear2Wave: avoid gzdopen(dup(fileno(..))) which crashes on MSVC.\n'
    '\t\t\t   Read raw compressed bytes and decompress with inflate. */\n'
    '\t\t\t{\n'
    '\t\t\tchar *raw = (char *)malloc(lt->zfacnamesize);\n'
    '\t\t\tfseeko(lt->handle, pos, SEEK_SET);\n'
    '\t\t\tint nraw = fread(raw, 1, lt->zfacnamesize, lt->handle);\n'
    '\t\t\tm = (char *)malloc(lt->zfacname_predec_size);\n'
    '\t\t\tz_stream zs = {0};\n'
    '\t\t\tzs.next_in = (Bytef *)raw;\n'
    '\t\t\tzs.avail_in = nraw;\n'
    '\t\t\tzs.next_out = (Bytef *)m;\n'
    '\t\t\tzs.avail_out = lt->zfacname_predec_size;\n'
    '\t\t\tint zrc = inflateInit2(&zs, 16 + MAX_WBITS);\n'
    '\t\t\tif (zrc == Z_OK) { zrc = inflate(&zs, Z_FINISH); rc = (int)zs.total_out; }\n'
    '\t\t\telse { rc = -1; }\n'
    '\t\t\tinflateEnd(&zs);\n'
    '\t\t\tfree(raw);\n'
    '\t\t\t}\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] inflate facnames: rc=%d expected=%d\\n",\n'
    '\t\t\t\trc, (int)lt->zfacname_predec_size); fflush(stderr);\n'
    '\n'
    '\t\t\tif(((lxtint32_t)rc)!=lt->zfacname_predec_size)'
)

# --- Second replacement: geometry decompression ---
# We need to match from the 'seek to geometry' line up to 'if(rc!=t)'
old2_pattern = (
    'seek to geometry: pos=%lld zfacgeometrysize=%d'
)

new2_block = (
    '\t\t\tfprintf(stderr, "[LXT2-DBG] seek to geometry: pos=%lld zfacgeometrysize=%d\\n",\n'
    '\t\t\t\t(long long)pos, (int)lt->zfacgeometrysize); fflush(stderr);\n'
    '\t\t\t/* Bear2Wave: avoid gzdopen, use inflate directly. */\n'
    '\t\t\t{\n'
    '\t\t\tchar *raw2 = (char *)malloc(lt->zfacgeometrysize);\n'
    '\t\t\tfseeko(lt->handle, pos, SEEK_SET);\n'
    '\t\t\tint nraw2 = fread(raw2, 1, lt->zfacgeometrysize, lt->handle);\n'
    '\t\t\tm = (char *)malloc(t);\n'
    '\t\t\tz_stream zs2 = {0};\n'
    '\t\t\tzs2.next_in = (Bytef *)raw2;\n'
    '\t\t\tzs2.avail_in = nraw2;\n'
    '\t\t\tzs2.next_out = (Bytef *)m;\n'
    '\t\t\tzs2.avail_out = t;\n'
    '\t\t\tint zrc2 = inflateInit2(&zs2, 16 + MAX_WBITS);\n'
    '\t\t\tif (zrc2 == Z_OK) { zrc2 = inflate(&zs2, Z_FINISH); rc = (int)zs2.total_out; }\n'
    '\t\t\telse { rc = -1; }\n'
    '\t\t\tinflateEnd(&zs2);\n'
    '\t\t\tfree(raw2);\n'
    '\t\t\t}\n'
    '\t\t\tfprintf(stderr, "[LXT2-DBG] inflate geometry: rc=%d expected=%d\\n", (int)rc, (int)t); fflush(stderr);\n'
    '\t\t\tif(rc!=t)'
)

if old1 in content:
    content = content.replace(old1, new1)
    print('[OK] First replacement (facility names)')
else:
    print('[FAIL] First replacement string not found!')
    # Search for partial match
    if 'masks allocated, about to gzdopen' in content:
        print('  -> Found "masks allocated" line')
        idx = content.find('masks allocated, about to gzdopen')
        snippet = content[idx:idx+800]
        print('  -> Context:\n' + repr(snippet[:500]))
    else:
        print('  -> "masks allocated" line NOT found')

# For geometry: find the section between 'geometry dup:' and 'if(rc!=t)'
dup_idx = content.find('geometry dup: raw=')
if dup_idx >= 0:
    print(f'[OK] Found geometry dup at offset {dup_idx}')
    # Find start of the geometry section (go back to 'seek to geometry')
    seek_start = content.rfind('\t\t\tfprintf(stderr, "[LXT2-DBG] seek to geometry', 0, dup_idx)
    if seek_start < 0:
        seek_start = content.rfind('seek to geometry: pos=', 0, dup_idx)
    if seek_start < 0:
        # try a bit earlier
        seek_start = content.rfind('fseeko(lt->handle, pos = pos+lt->zfacnamesize', 0, dup_idx)

    # Find end (the 'if(rc!=t)' line)
    rc_end = content.find('if(rc!=t)', dup_idx)
    if rc_end < 0:
        rc_end = content.find('if((rc!=t)', dup_idx)

    if seek_start >= 0 and rc_end >= 0:
        # Find the actual line start for seek_start
        line_start = content.rfind('\n', 0, seek_start) + 1
        # Find the line end for rc_end (include the line)
        line_end = content.find('\n', rc_end) + 1
        old2 = content[line_start:line_end]
        print(f'  Replacing geometry section ({line_end - line_start} bytes)')

        # Build the replacement for geometry
        # We need to prepend the "seek to geometry" log line
        replacement = new2_block
        content = content[:line_start] + replacement + content[line_end:]
        print('[OK] Second replacement (geometry)')
    else:
        print(f'  seek_start={seek_start} rc_end={rc_end}')
else:
    print('[FAIL] geometry dup section not found!')

# Also remove the gzclose references (they set zhandle to NULL, but we're not using it anymore)
# The zhandle is used later for block decompression - need to check

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print('Done writing file.')
