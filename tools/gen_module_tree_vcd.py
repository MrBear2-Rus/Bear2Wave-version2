#!/usr/bin/env python3
"""Generate VCD with many modules for lazy tree stress (P4-6)."""

from __future__ import annotations

import argparse
import os


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", default=os.path.join("tests", "traces", "module_tree_stress.vcd"))
    parser.add_argument("--modules", type=int, default=2000)
    parser.add_argument("--depth", type=int, default=3)
    args = parser.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.o)), exist_ok=True)

    with open(args.o, "w", encoding="ascii", newline="\n") as f:
        f.write("$date gen_module_tree_vcd $end\n$version 1.0 $end\n$timescale 1ns $end\n")
        ids = []
        for i in range(args.modules):
            depth = (i % args.depth) + 1
            path = ".".join(f"m{j}" for j in range(depth))
            sid = f"s{i % 94}"
            f.write(f"$scope module {path} $end\n")
            f.write(f"$var wire 1 {sid} bit_{i} $end\n")
            f.write("$upscope $end\n")
            ids.append(sid)
        f.write("$enddefinitions $end\n")
        for t in range(0, 41, 10):
            f.write(f"#{t}\n")
            for sid in ids[: min(32, len(ids))]:
                f.write(f"{(t // 10) & 1}{sid}\n")

    print(f"wrote {args.o} modules={args.modules} depth={args.depth}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
