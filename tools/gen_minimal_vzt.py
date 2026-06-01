#!/usr/bin/env python3
r"""
Generate and inspect a tiny valid VZT sample without requiring msbuild.

Usage:
  py tools\gen_minimal_vzt.py generate tests\traces\bear2wave_sample.vzt
  py tools\gen_minimal_vzt.py describe tests\traces\bear2wave_sample.vzt
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


VZT_HDR_ID = 0x565A
VZT_VERSION = 1
VZT_GRANULE_SIZE = 32
VZT_WIRE_FLAG = 1 << 15


def be16(value: int) -> bytes:
    return struct.pack(">H", value & 0xFFFF)


def be32(value: int) -> bytes:
    return struct.pack(">I", value & 0xFFFFFFFF)


def be64(value: int) -> bytes:
    return struct.pack(">Q", value & 0xFFFFFFFFFFFFFFFF)


def le32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def uv(value: int) -> bytes:
    if value < 0:
        raise ValueError("uv encoding only supports non-negative integers")
    out = bytearray()
    while True:
        nxt = value >> 7
        if nxt:
            out.append(value & 0x7F)
            value = nxt
        else:
            out.append((value & 0x7F) | 0x80)
            return bytes(out)


def gzip_bytes(data: bytes) -> bytes:
    """Gzip wrapper compatible with GTKWave/libz (zlib wbits=16+MAX_WBITS)."""
    return zlib.compress(data, level=9, wbits=16 + zlib.MAX_WBITS)


def build_name_section(full_name: str) -> bytes:
    return be16(0) + full_name.encode("utf-8") + b"\x00"


def build_geometry_section() -> bytes:
    return b"".join(
        [
            be32(1),
            be32(0),
            be32(0),
            be32(VZT_WIRE_FLAG),
        ]
    )


def build_block_section() -> bytes:
    body = bytearray()

    body += uv(2)   # two timestamps
    body += uv(0)   # first time = 0
    body += uv(10)  # delta to second time = 10
    body += uv(1)   # one 32-bit section
    body += uv(2)   # two dictionary entries: 0xFFFFFFFE and 0
    while len(body) & 3:
        body.append(0)

    body += le32(0xFFFFFFFE)
    body += le32(0x00000000)

    body += b"\x00"  # one bitplane total (two-state)
    while len(body) & 3:
        body.append(0)

    body += le32(0)  # signal points at dictionary entry 0
    body += uv(0)    # no string table
    return bytes(body)


def build_vzt() -> bytes:
    names_raw = build_name_section("TOP.clk")
    geom_raw = build_geometry_section()
    block_raw = build_block_section()

    names_gz = gzip_bytes(names_raw)
    geom_gz = gzip_bytes(geom_raw)
    block_gz = gzip_bytes(block_raw)

    header = bytearray()
    header += be16(VZT_HDR_ID)
    header += be16(VZT_VERSION)
    header += struct.pack("B", VZT_GRANULE_SIZE)
    header += be32(1)                 # numfacs
    header += be32(len("TOP.clk") + 1)
    header += be32(len("TOP.clk"))
    header += be32(len(names_gz))
    header += be32(len(names_raw))
    header += be32(len(geom_gz))
    header += struct.pack("B", 9)     # ns

    block_header = bytearray()
    block_header += be32(len(block_raw))
    block_header += be32(len(block_gz))
    block_header += be64(0)
    block_header += be64(10)

    return bytes(header) + names_gz + geom_gz + bytes(block_header) + block_gz


def read_be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def read_be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def read_be64(data: bytes, offset: int) -> int:
    return struct.unpack_from(">Q", data, offset)[0]


def describe_vzt(path: Path) -> int:
    data = path.read_bytes()
    if len(data) < 31:
        raise SystemExit(f"{path} is too small to be a VZT file")

    hdr_id = read_be16(data, 0)
    version = read_be16(data, 2)
    granule = data[4]
    numfacs = read_be32(data, 5)
    numfacbytes = read_be32(data, 9)
    longestname = read_be32(data, 13)
    zfacnamesize = read_be32(data, 17)
    zfacname_predec_size = read_be32(data, 21)
    zfacgeometrysize = read_be32(data, 25)
    timescale = data[29]

    pos = 30
    names_gz = data[pos:pos + zfacnamesize]
    pos += zfacnamesize
    geom_gz = data[pos:pos + zfacgeometrysize]
    pos += zfacgeometrysize

    names_raw = gzip.decompress(names_gz)
    geom_raw = gzip.decompress(geom_gz)

    if len(geom_raw) != numfacs * 16:
        raise SystemExit(
            f"{path} geometry size mismatch: got {len(geom_raw)} bytes, expected {numfacs * 16}"
        )

    if pos + 24 > len(data):
        raise SystemExit(f"{path} is missing block header")

    block_uncompressed = read_be32(data, pos + 0)
    block_compressed = read_be32(data, pos + 4)
    block_start = read_be64(data, pos + 8)
    block_end = read_be64(data, pos + 16)
    pos += 24

    block_gz = data[pos:pos + block_compressed]
    block_raw = gzip.decompress(block_gz)
    if len(block_raw) != block_uncompressed:
        raise SystemExit(
            f"{path} block size mismatch: got {len(block_raw)} bytes, expected {block_uncompressed}"
        )

    name_clone = read_be16(names_raw, 0)
    first_name = names_raw[2:].split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    rows = read_be32(geom_raw, 0)
    msb = read_be32(geom_raw, 4)
    lsb = read_be32(geom_raw, 8)
    flags = read_be32(geom_raw, 12)

    print(f"path={path}")
    print(f"size={len(data)}")
    print(f"header.id=0x{hdr_id:04X}")
    print(f"header.version={version}")
    print(f"header.granule={granule}")
    print(f"header.numfacs={numfacs}")
    print(f"header.numfacbytes={numfacbytes}")
    print(f"header.longestname={longestname}")
    print(f"header.zfacnamesize={zfacnamesize}")
    print(f"header.zfacname_predec_size={zfacname_predec_size}")
    print(f"header.zfacgeometrysize={zfacgeometrysize}")
    print(f"header.timescale={timescale}")
    print(f"name.clone_prefix={name_clone}")
    print(f"name.first={first_name}")
    print(f"geometry.rows={rows}")
    print(f"geometry.msb={msb}")
    print(f"geometry.lsb={lsb}")
    print(f"geometry.flags=0x{flags:08X}")
    print(f"block.uncompressed={block_uncompressed}")
    print(f"block.compressed={block_compressed}")
    print(f"block.start={block_start}")
    print(f"block.end={block_end}")
    return 0


def generate_vzt(path: Path) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = build_vzt()
    path.write_bytes(data)
    print(f"generated {path} ({len(data)} bytes)")
    return describe_vzt(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["generate", "describe"])
    parser.add_argument("path")
    args = parser.parse_args()

    path = Path(args.path)
    if args.command == "generate":
        return generate_vzt(path)
    return describe_vzt(path)


if __name__ == "__main__":
    raise SystemExit(main())
