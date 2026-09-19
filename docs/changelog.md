# Changelog

A chronological "why" log. For current architecture/spec, see
[`../CLAUDE.md`](../CLAUDE.md). For the reasoning behind the sensing
approach itself, see [`methodology.md`](methodology.md).

## 2026-09-18 — Repo overhaul: pivot to impedance-spectroscopy soil probe

The project started as a round-TFT display driven by a DHT11
temperature/humidity sensor (see `legacy/soil-moisture-meter.ino`) — no
actual soil sensing yet. Reframed the goal around extracting as much
information as possible (moisture, salinity/EC as a nutrient-trend proxy,
a soft soil-texture signal, temperature) from a single low-cost two-prong
soil probe, using electrical impedance spectroscopy rather than a
single-frequency reading.

Decisions made:

- Archive the display/DHT11 sketch as a reference rather than deleting it;
  it may become the eventual on-device UI stage later.
- Target an ESP32 dev board for the breadboard prototyping phase (cheap,
  BLE/WiFi built in, easy bring-up).
- Use an AD5933-class impedance-analyzer breakout for the probe front end
  rather than a discrete analog design, to get to real sweep data faster.

Repo restructured into `docs/` (methodology, hardware BOM/wiring,
changelog), `firmware/` (ESP32 sketch), `analysis/` (Python
calibration/signal-processing tooling), and `legacy/` (the original
sketch).
