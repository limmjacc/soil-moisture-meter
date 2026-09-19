#!/usr/bin/env python3
"""Turn a captured AD5933 sweep log into calibrated impedance, and pull out
the moisture/EC proxy signals described in docs/methodology.md.

Pipeline:
  raw log (from capture.py)
    -> parse_capture_log(): grouped sweeps of {freq_hz, real, imag} points
    -> calibrate_gain(): gain factor + phase offset per frequency, fit
       against a sweep of a KNOWN reference resistor (see docs/hardware/bom.md)
    -> apply_calibration(): raw counts -> actual impedance magnitude/phase
       -> real (R) and reactive (X) components
    -> extract_features(): R/X -> the moisture/EC/texture proxies

The AD5933 datasheet's own gain-factor calibration procedure assumes a
single reference resistor near the expected impedance magnitude; soil
impedance swings widely with moisture, so calibrate against whichever
reference resistor from the BOM is closest to the impedance range actually
being measured, and re-calibrate if that range shifts a lot.

extract_features() is a placeholder: the constants that would turn these
proxies into real VWC (%) and EC (dS/m) figures require empirical
calibration against known soil samples (air, dry sand, saturated sand,
plus known-salinity wet samples) that have not been collected yet — do not
invent calibration constants here. See docs/methodology.md's "Open items"
section.
"""
import argparse
import csv
import math
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class SweepPoint:
    freq_hz: int
    real: int
    imag: int


@dataclass
class Sweep:
    env: dict
    points: list = field(default_factory=list)


def parse_capture_log(path):
    """Groups a raw capture.py log into Sweep objects.

    The firmware prints one '# env,...' comment line at the start of each
    loop iteration, followed by that sweep's data rows, so an env line
    always marks the start of a new sweep.
    """
    sweeps = []
    current = None

    with open(path, newline="") as f:
        reader = csv.reader(f)
        next(reader, None)  # header: captured_at,line
        for _captured_at, line in reader:
            if line.startswith("# env,"):
                if current is not None and current.points:
                    sweeps.append(current)
                env = {}
                for kv in line[len("# env,"):].split(","):
                    key, _, value = kv.partition("=")
                    try:
                        env[key] = float(value)
                    except ValueError:
                        env[key] = value
                current = Sweep(env=env)
            elif line.startswith("#"):
                continue  # header / sweep_error / other comment lines
            else:
                if current is None:
                    continue  # data row before the first env line; discard
                ts_ms, freq_hz, real, imag = line.split(",")
                current.points.append(
                    SweepPoint(int(freq_hz), int(real), int(imag))
                )

    if current is not None and current.points:
        sweeps.append(current)

    return sweeps


def _magnitude_phase(point):
    magnitude = math.hypot(point.real, point.imag)
    phase = math.atan2(point.imag, point.real)
    return magnitude, phase


def calibrate_gain(reference_sweep, r_known_ohm):
    """Fits a per-frequency gain factor + phase offset from a sweep taken
    across a known precision resistor (see docs/hardware/bom.md) in place
    of the probe. An ideal resistor has zero phase, so the raw phase
    measured here IS the system's phase offset to subtract later.

    Returns {freq_hz: (gain_factor, phase_offset_rad)}.
    """
    table = {}
    for point in reference_sweep.points:
        magnitude_raw, phase_raw = _magnitude_phase(point)
        if magnitude_raw == 0:
            continue  # bad/missing reading; leave this frequency uncalibrated
        gain_factor = 1.0 / (magnitude_raw * r_known_ohm)
        table[point.freq_hz] = (gain_factor, phase_raw)
    return table


@dataclass
class CalibratedPoint:
    freq_hz: int
    magnitude_ohm: float
    phase_rad: float
    resistance_ohm: float  # R, real part — EC-related
    reactance_ohm: float  # X, imaginary part — moisture-related
    capacitance_f: Optional[float]  # equivalent parallel C, when reactance is capacitive


def apply_calibration(sweep, gain_table):
    """Converts a raw sweep to calibrated impedance using a gain table from
    calibrate_gain(). Assumes the sweep was captured with the same firmware
    frequency configuration as the reference sweep (matching freq_hz keys) —
    if you change SWEEP_START_HZ/SWEEP_INCR_HZ/SWEEP_NUM_INCR in the
    firmware, recapture a reference sweep before recalibrating.
    """
    calibrated = []
    for point in sweep.points:
        if point.freq_hz not in gain_table:
            continue  # no calibration data at this frequency; skip rather than guess
        gain_factor, phase_offset = gain_table[point.freq_hz]

        magnitude_raw, phase_raw = _magnitude_phase(point)
        if magnitude_raw == 0 or gain_factor == 0:
            continue

        magnitude_ohm = 1.0 / (gain_factor * magnitude_raw)
        phase_rad = phase_raw - phase_offset

        resistance_ohm = magnitude_ohm * math.cos(phase_rad)
        reactance_ohm = magnitude_ohm * math.sin(phase_rad)

        capacitance_f = None
        if reactance_ohm < 0:  # capacitive reactance is negative by convention
            capacitance_f = -1.0 / (2 * math.pi * point.freq_hz * reactance_ohm)

        calibrated.append(
            CalibratedPoint(
                point.freq_hz,
                magnitude_ohm,
                phase_rad,
                resistance_ohm,
                reactance_ohm,
                capacitance_f,
            )
        )
    return calibrated


def extract_features(calibrated_points):
    """Placeholder feature extraction — see module docstring. Returns raw
    proxy quantities, NOT calibrated physical units (not %, not dS/m).
    """
    if not calibrated_points:
        return {}

    lowest = min(calibrated_points, key=lambda p: p.freq_hz)
    highest = max(calibrated_points, key=lambda p: p.freq_hz)

    return {
        # TODO(calibration): fit against known-salinity reference samples
        # to convert to actual EC (dS/m).
        "ec_proxy_resistance_ohm": lowest.resistance_ohm,
        # TODO(calibration): fit against known-VWC reference samples
        # (air / dry sand / saturated sand) to convert to VWC (%).
        "moisture_proxy_capacitance_f": highest.capacitance_f,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_log", help="capture.py log taken across a known resistor")
    parser.add_argument("r_known_ohm", type=float, help="that resistor's value in ohms")
    parser.add_argument("probe_log", help="capture.py log taken across the soil probe")
    args = parser.parse_args()

    reference_sweeps = parse_capture_log(args.reference_log)
    if not reference_sweeps:
        raise SystemExit(f"No sweeps found in {args.reference_log}")
    gain_table = calibrate_gain(reference_sweeps[0], args.r_known_ohm)

    probe_sweeps = parse_capture_log(args.probe_log)
    for i, sweep in enumerate(probe_sweeps):
        calibrated = apply_calibration(sweep, gain_table)
        features = extract_features(calibrated)
        print(f"Sweep {i} — env: {sweep.env}")
        for point in calibrated:
            print(
                f"  {point.freq_hz:>7d} Hz  |Z|={point.magnitude_ohm:10.1f} ohm  "
                f"R={point.resistance_ohm:10.1f}  X={point.reactance_ohm:10.1f}"
            )
        print(f"  features: {features}")


if __name__ == "__main__":
    main()
