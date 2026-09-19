# soil-moisture-meter

A small BLE device meant to be inserted into soil and read from a nearby
phone, aiming to extract as much useful information as possible — water
content, salinity/nutrient trend, a soft soil-texture signal, and
temperature — from a single low-cost two-electrode probe, plus ambient air
temperature/humidity.

## Approach, in short

Rather than taking one DC or single-frequency reading across two prongs
(what cheap off-the-shelf "soil moisture modules" do), this project drives
the same two electrodes as an electrical impedance spectrometer: an AC
excitation sweep across frequency, capturing magnitude and phase at each
point. Different physical effects dominate different parts of that
spectrum, which lets several independent quantities be pulled out of one
probe:

- **Volumetric water content** — from the capacitive component at
  moderate-to-high frequency.
- **Bulk electrical conductivity (EC)** — from the resistive component,
  correlating with salinity/dissolved fertilizer salts (a trend signal,
  not an absolute nutrient reading — see caveats below).
- **A soft soil-texture signal** — from low-frequency dielectric dispersion
  shape, calibration-dependent and still speculative.

**Important limitation:** this technique cannot produce true NPK
concentrations or pH — those require ion-selective electrodes or
optical/colorimetric chemistry, a different sensing approach entirely.
Bulk EC is a reasonable *trend* indicator (is fertilizer load rising or
falling), never an absolute nutrient measurement. See
[`docs/methodology.md`](docs/methodology.md) for the full reasoning,
including why AC-only excitation (rather than DC) was chosen to avoid
corroding the probe electrodes.

## Status

**Phase 1: breadboard prototyping.** No PCB, no enclosure, no battery yet —
validating the impedance-sweep signal chain on an ESP32 + AD5933-class
impedance analyzer breakout, powered over USB. See
[`docs/changelog.md`](docs/changelog.md) for how the project got here.

## Repo layout

```
docs/       Methodology, hardware BOM/wiring, changelog — start at docs/README.md
firmware/   ESP32 firmware: AD5933 sweep driver, sensor reads, CSV over serial
analysis/   Python tooling: serial capture, calibration, impedance decomposition
legacy/     Earlier round-TFT + DHT11 display sketch, kept as reference
```

## Building this yourself

1. Read [`docs/methodology.md`](docs/methodology.md) for the reasoning
   behind the sensing approach.
2. Gather parts per [`docs/hardware/bom.md`](docs/hardware/bom.md)
   (breadboard-phase cost: roughly $25-35 in new parts).
3. Wire it up per [`docs/hardware/wiring.md`](docs/hardware/wiring.md),
   following its bring-up order (validate with a known resistor before
   introducing the actual soil probe).
4. Flash [`firmware/esp32_probe/esp32_probe.ino`](firmware/esp32_probe/esp32_probe.ino).
   This is first-bring-up firmware, not yet verified against physical
   hardware — see the caveats at the top of the file before trusting
   absolute impedance values.
5. Use `analysis/capture.py` to log sweep data over serial, then
   `analysis/calibration.py` to calibrate it against a known reference
   resistor and pull out the moisture/EC proxy signals (see the module
   docstring for the current calibration limitations).

## License

No license file yet — all rights reserved by default until one is added.
