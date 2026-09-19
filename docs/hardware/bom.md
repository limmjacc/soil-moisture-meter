# Prototyping bill of materials (breadboard phase)

Goal for this phase: cheapest path to real frequency-sweep data from a soil
probe, on a breadboard, before any PCB work. No enclosure, no battery, no
BLE polish yet — USB power from a computer is sufficient.

Search terms are given instead of links (part numbers/models are specific
enough to search directly on whatever supplier you prefer — AliExpress,
Amazon, eBay, Adafruit, SparkFun, DigiKey all carry equivalents at very
different price points; generic modules are fine for this phase).

| Part | Purpose | Search term | Approx. cost |
|---|---|---|---|
| ESP32 dev board (WROOM-32, DevKitC-style) | Main MCU, I2C master for AD5933, ADC for thermistor, future BLE | "ESP32 DevKit V1 30 pin" | $5–8 |
| AD5933 breakout module | Impedance analyzer front end — I2C-driven frequency sweep with onboard DFT | "AD5933 evaluation breakout module" | $10–15 |
| Stainless steel probe electrodes (2x) | The soil-contact probe itself | "stainless steel welding rod" (cut to length) or "M4 stainless steel bolts" | $2–3 for a usable supply |
| Precision resistors, 1%, a few values spanning 1kΩ–100kΩ | AD5933 gain/phase calibration references (soil impedance varies a lot with moisture; a couple of known resistors bracketing the expected range let the calibration code interpolate a gain factor) | "1% metal film resistor kit" | $5–8 for an assortment kit (likely already have some) |
| NTC thermistor, 10kΩ | Soil temperature at the probe tip | "10k NTC thermistor 3950" (the type widely used in 3D printer hot ends) | $1–2 for a pack |
| Fixed resistor, ~10kΩ, 1% | Thermistor voltage-divider pair | included in the resistor kit above | — |
| DHT11 | Ambient air temperature/humidity (already owned from the earlier display prototype — see `legacy/`) | — (reuse existing part) | $0 |
| Breadboard + jumper wire kit | Prototyping | "solderless breadboard jumper wire kit" | $5–8 (likely already have from the earlier prototype) |
| USB cable (micro-USB or USB-C, matching the dev board) | Power + serial link to computer | — | $0–3 (likely already have) |

**Estimated new spend, assuming breadboard/jumpers/DHT11 are already on
hand from the earlier display prototype: roughly $25–35.**

## Notes on part choices

- **AD5933 breakout, not a discrete circuit.** A discrete signal-generator
  + synchronous-detection front end would be a few dollars cheaper in
  parts, but adds substantial analog design/debug time before any usable
  data exists. See [`../methodology.md`](../methodology.md) for the
  reasoning. Revisit discrete designs once the AD5933 path has produced
  validated calibration data, if BOM cost becomes the binding constraint
  for a production version.
- **Calibration resistors, not just one.** The AD5933 datasheet's
  gain-factor calibration procedure uses a single known resistor near the
  expected impedance magnitude, but soil impedance swings over a wide range
  with moisture (dry soil can be hundreds of kΩ; saturated soil can be a
  few kΩ). Having 2–3 reference resistors spanning that range makes it
  possible to check (and if needed, interpolate) gain factor across the
  sweep rather than trusting a single calibration point outside its valid
  range.
- **Stainless steel over bare copper/brass for the probe tips.** Bare
  copper corrodes quickly in moist, ionic soil even under AC-only
  excitation; stainless steel is the cheap, readily available
  corrosion-resistant option for a breadboard prototype. (Repurposing the
  bare metal fork from a cheap off-the-shelf resistive soil moisture
  module is a viable zero-additional-cost source for a first probe, if one
  is already on hand — just ignore the module's onboard comparator
  circuit and wire straight to the two prongs.)
- **No multiplexer/relay yet.** Automatically switching between the probe
  and a calibration resistor is convenient but not necessary for this
  phase — manually swapping which two points the AD5933's excitation
  output/receive input are wired to is fine while validating the
  measurement pipeline. Revisit once repeated calibration runs become
  tedious.

## Deferred to a later phase (not needed yet)

- BLE antenna/enclosure considerations — the ESP32 dev board's onboard
  antenna is sufficient for breadboard-range testing.
- Battery and power management (coin cell, LDO, sleep current budgeting) —
  only relevant once moving toward a buried, untethered final device.
- A better ambient RH/temp sensor than the DHT11 (e.g., SHT31) — the DHT11
  already on hand is accurate enough to validate the overall data pipeline;
  swap later if ambient-humidity accuracy turns out to matter.
