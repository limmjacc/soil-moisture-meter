#!/usr/bin/env python3
"""Log the ESP32 probe's raw serial stream to a timestamped CSV file.

Every line (data rows and '#'-prefixed comment/env/error lines alike) is
kept verbatim with a wall-clock timestamp added — calibration.py parses
this raw log format directly, since the env/data line interleaving carries
meaning (see firmware/esp32_probe/esp32_probe.ino's loop()).
"""
import argparse
import csv
import datetime

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial port, e.g. /dev/cu.usbserial-0001 or COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--out",
        help="Output CSV path (default: data/sweep_<timestamp>.csv)",
    )
    args = parser.parse_args()

    out_path = args.out or f"data/sweep_{datetime.datetime.now():%Y%m%d_%H%M%S}.csv"

    with serial.Serial(args.port, args.baud, timeout=5) as ser, open(
        out_path, "w", newline=""
    ) as f:
        writer = csv.writer(f)
        writer.writerow(["captured_at", "line"])
        print(f"Logging {args.port} -> {out_path} (Ctrl+C to stop)")
        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip()
                if not line:
                    continue
                writer.writerow([datetime.datetime.now().isoformat(), line])
                f.flush()
                print(line)
        except KeyboardInterrupt:
            print(f"\nStopped. Saved to {out_path}")


if __name__ == "__main__":
    main()
