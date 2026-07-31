#!/usr/bin/env python3
"""Capture and summarize the binary ball-position stream mirrored on COM9."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
import time
from pathlib import Path

import serial

FRAME_LEN = 13
FRAME_TYPE = 0x02
CONF_MIN = 30


def parse_frames(port: serial.Serial, duration: float) -> list[dict[str, float | int]]:
    records: list[dict[str, float | int]] = []
    frame = bytearray(FRAME_LEN)
    state = 0
    index = 0
    start = time.monotonic()

    while time.monotonic() - start < duration:
        data = port.read(max(1, min(port.in_waiting, 256)))
        if not data:
            continue
        for value in data:
            if state == 0:
                if value == 0xAA:
                    frame[0] = value
                    state = 1
                continue
            if state == 1:
                if value == 0x55:
                    frame[1] = value
                    index = 2
                    state = 2
                elif value != 0xAA:
                    state = 0
                continue

            frame[index] = value
            index += 1
            if index != FRAME_LEN:
                continue

            checksum = sum(frame[2:12]) & 0xFF
            if checksum == frame[12] and frame[2] == FRAME_TYPE:
                records.append(
                    {
                        "time_s": time.monotonic() - start,
                        "found": int(bool(frame[3] & 0x01)),
                        "pos_mm": int.from_bytes(frame[4:6], "little", signed=True),
                        "cx": int.from_bytes(frame[6:8], "little", signed=True),
                        "cy": int.from_bytes(frame[8:10], "little", signed=True),
                        "conf": frame[10],
                        "mode": frame[11],
                    }
                )
            state = 0

    return records


def summarize(records: list[dict[str, float | int]]) -> None:
    usable = [
        record
        for record in records
        if record["found"] and int(record["conf"]) >= CONF_MIN
    ]
    print(f"frames={len(records)} usable={len(usable)} lost={len(records) - len(usable)}")
    if not usable:
        return

    positions = [int(record["pos_mm"]) for record in usable]
    crossings = sum(
        (left < 0 <= right) or (left > 0 >= right)
        for left, right in zip(positions, positions[1:])
    )
    tail = positions[-min(60, len(positions)) :]
    tail_abs = [abs(value) for value in tail]
    print(
        "start={0} end={1} min={2} max={3} mean={4:.1f} "
        "crossings={5} tail_mean_abs={6:.1f} tail_max_abs={7}".format(
            positions[0],
            positions[-1],
            min(positions),
            max(positions),
            statistics.fmean(positions),
            crossings,
            statistics.fmean(tail_abs),
            max(tail_abs),
        )
    )

    for second in range(math.floor(float(usable[-1]["time_s"])) + 1):
        bucket = [
            int(record["pos_mm"])
            for record in usable
            if second <= float(record["time_s"]) < second + 1
        ]
        if bucket:
            print(
                f"t={second:02d}s n={len(bucket):02d} "
                f"mean={statistics.fmean(bucket):6.1f} "
                f"min={min(bucket):4d} max={max(bucket):4d}"
            )


def write_csv(path: Path, records: list[dict[str, float | int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = ["time_s", "found", "pos_mm", "cx", "cy", "conf", "mode"]
    with path.open("w", newline="", encoding="ascii") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(records)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", nargs="?", default="COM9")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--output", type=Path, default=Path("build/ball_trace.csv"))
    args = parser.parse_args()

    print(f"listening on {args.port} for {args.duration:.1f}s")
    with serial.Serial(args.port, 115200, timeout=0.1) as port:
        records = parse_frames(port, args.duration)
    write_csv(args.output, records)
    summarize(records)
    print(f"csv={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
