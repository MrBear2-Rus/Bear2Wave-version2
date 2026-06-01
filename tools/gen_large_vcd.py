#!/usr/bin/env python3
"""Generate a large VCD for Bear2Wave lazy-load / M3 testing."""

from __future__ import annotations

import argparse
import os
import sys
import time


def vcd_id(index: int) -> str:
    """Stable short VCD identifier (single printable char when possible)."""
    chars = (
        "!\"#$%&'()*+,-./0123456789:;<=>?@"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    )
    if index < len(chars):
        return chars[index]
    return f"s{index}"


def write_header(
    out,
    modules: int,
    signals_per_module: int,
    bus_width: int,
) -> list[tuple[str, bool]]:
    out.write("$date\n    Bear2Wave gen_large_vcd\n$end\n")
    out.write("$version\n    gen_large_vcd 1.0\n$end\n")
    out.write("$timescale 1ns $end\n")

    meta: list[tuple[str, bool]] = []
    sig_idx = 0
    for m in range(modules):
        out.write(f"$scope module mod_{m} $end\n")
        for s in range(signals_per_module):
            sid = vcd_id(sig_idx)
            sig_idx += 1
            is_bus = (s % 5 == 0) and bus_width > 1
            if is_bus:
                out.write(
                    f"$var wire {bus_width} {sid} bus_{m}_{s}[{bus_width - 1}:0] $end\n"
                )
            else:
                out.write(f"$var wire 1 {sid} bit_{m}_{s} $end\n")
            meta.append((sid, is_bus))
        out.write("$upscope $end\n")
    out.write("$enddefinitions $end\n")
    return meta


def estimate_bytes_per_step(meta: list[tuple[str, bool]], bus_width: int) -> int:
    total = 12  # e.g. "#1234567890\n"
    for sid, is_bus in meta:
        if is_bus:
            total += 2 + bus_width + len(sid)  # b<bits> <id>\n approx
        else:
            total += 2 + len(sid)  # 0<id>\n
    return total


def generate_data(
    out,
    meta: list[tuple[str, bool]],
    *,
    step_ns: int,
    num_steps: int,
    bus_width: int,
    flush_lines: int,
) -> None:
    lines: list[str] = []

    def flush() -> None:
        if lines:
            out.writelines(lines)
            lines.clear()

    for step in range(num_steps + 1):
        ts = step * step_ns
        lines.append(f"#{ts}\n")
        bit = "1" if (step & 1) else "0"
        bus_val = format(step & ((1 << bus_width) - 1), f"0{bus_width}b")
        for sid, is_bus in meta:
            if is_bus:
                lines.append(f"b{bus_val} {sid}\n")
            else:
                lines.append(f"{bit}{sid}\n")
        if len(lines) >= flush_lines:
            flush()
    flush()


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a large VCD trace file.")
    parser.add_argument(
        "-o",
        "--output",
        default=os.path.join("tests", "traces", "large_test.vcd"),
        help="Output .vcd path (default: tests/traces/large_test.vcd)",
    )
    parser.add_argument(
        "--size-mb",
        type=float,
        default=100.0,
        help="Approximate target file size in MiB (default: 100)",
    )
    parser.add_argument("--modules", type=int, default=32, help="Number of modules")
    parser.add_argument(
        "--signals-per-module",
        type=int,
        default=32,
        help="Signals per module",
    )
    parser.add_argument("--bus-width", type=int, default=32, help="Width of bus signals")
    parser.add_argument("--step-ns", type=int, default=10, help="Time step in ns")
    parser.add_argument(
        "--steps",
        type=int,
        default=0,
        help="Exact number of time steps (overrides --size-mb)",
    )
    args = parser.parse_args()

    out_path = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    t0 = time.time()
    with open(out_path, "w", encoding="ascii", newline="\n") as f:
        meta = write_header(f, args.modules, args.signals_per_module, args.bus_width)
        header_bytes = f.tell()
        bps = estimate_bytes_per_step(meta, args.bus_width)
        if args.steps > 0:
            num_steps = args.steps
        else:
            target = int(args.size_mb * 1024 * 1024)
            data_budget = max(target - header_bytes, bps)
            num_steps = max(1, data_budget // bps)

        print(
            f"[gen_large_vcd] signals={len(meta)} modules={args.modules} "
            f"step_ns={args.step_ns} steps={num_steps} (~{bps} B/step)",
            file=sys.stderr,
        )
        generate_data(
            f,
            meta,
            step_ns=args.step_ns,
            num_steps=num_steps,
            bus_width=args.bus_width,
            flush_lines=8192,
        )

    size = os.path.getsize(out_path)
    elapsed = time.time() - t0
    print(
        f"[gen_large_vcd] wrote {out_path}\n"
        f"  size={size / (1024 * 1024):.2f} MiB ({size} bytes)\n"
        f"  max_time={num_steps * args.step_ns} ns\n"
        f"  elapsed={elapsed:.1f}s",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
