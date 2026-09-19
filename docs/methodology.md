# Methodology: multi-parameter sensing from a two-electrode soil probe

This document records the reasoning behind the project's sensing approach —
what a minimal two-electrode probe can and cannot measure, and why the
design landed where it did. It is a narrative/rationale document; the
current, lean hardware spec lives in [`CLAUDE.md`](../CLAUDE.md).

## Goal

Extract as much useful information as possible about soil (and its
immediate environment) from as few physical probes as possible, in a
device cheap enough to be a consumer product, that can hand readings to a
phone over BLE when nearby.

## Core technique: electrical impedance spectroscopy (EIS), not a single reading

A generic off-the-shelf "soil moisture module" takes one DC or single-frequency
AC reading across two prongs and reports a single number. The two prongs
themselves are not the limiting factor — the excitation is. If the same two
electrodes are driven with AC excitation swept across a range of
frequencies, and both the magnitude and phase of the resulting impedance
are captured at each frequency, the probe becomes a small impedance
spectrometer. That single sweep carries several independent pieces of
information, separable because they dominate different parts of the
frequency spectrum.

### What a frequency sweep across two electrodes can extract

- **Volumetric water content (VWC)** — from the capacitive (imaginary)
  component of impedance at moderate-to-high frequency (roughly tens of kHz
  to a few MHz). This is the same physical principle used by commercial
  capacitance soil moisture sensors and TDR probes: water dominates the
  bulk dielectric permittivity of soil far more than the solid mineral
  fraction or air does.
- **Bulk electrical conductivity (EC)** — from the resistive (real)
  component of impedance, generally at lower frequency. EC is driven by the
  concentration of dissolved ions in the soil solution (this is the same
  quantity fertilizer salts and general salinity register on). EC is only
  meaningful *relative to* water content — a dry, salty soil and a wet,
  low-salt soil can produce similar raw resistive readings — so VWC and EC
  must always be reported together, never independently.
- **A soft signal for soil texture (sand/silt/clay fraction)** — clay
  content produces distinct low-frequency dielectric dispersion
  (interfacial / Maxwell-Wagner polarization at the mineral-water
  interface), which shows up as curvature in the frequency-dependent
  impedance response (a Cole-Cole-type relaxation). This is a soft
  classifier that needs calibration against soil samples of known texture —
  not a precise measurement, and it is the most speculative of the three.

### What it cannot extract — and should not be marketed as extracting

- **True NPK concentration.** Bulk conductivity is not nutrient-specific;
  it cannot distinguish nitrate from potassium from generic salt. Real
  nutrient quantification requires ion-selective electrodes (ISEs) or
  optical/colorimetric chemistry — a fundamentally different sensing
  modality. Many inexpensive "soil NPK meter" products imply single-number
  NPK readings from bulk conductivity or even from unpowered
  galvanic-cell circuits; these are not physically defensible as absolute
  readings. EC can support a *trend* indicator (fertilizer load rising or
  falling over time, relative to a baseline for that soil) but not an
  absolute ppm figure. Be explicit about this distinction in any user-facing
  copy.
- **pH.** Requires a dedicated ion-selective (glass or ISFET) electrode.
  Not obtainable from bulk impedance.
- **Air humidity.** Comes from a separate transducer exposed to ambient
  air on the housing (e.g., a small capacitive RH sensor), not from
  anything in the soil. It does not consume a second soil probe — it is a
  different sensor entirely, just packaged in the same device.
- **Soil temperature**, reliably. Impedance is temperature-dependent, but
  that dependence is confounded with moisture and salinity in a single
  two-electrode measurement. A small thermistor at the probe tip is a
  cheap, unambiguous way to get temperature without trying to untangle it
  from the impedance data.

## Electrode excitation: the choice that determines electrode lifetime

This matters as much as electrode count:

- **Bare metal, DC-driven** (the cheapest resistive modules on the market) —
  electrolyzes the probe metal and the soil moisture around it, causing
  corrosion and drift within weeks. This is the known failure mode that
  pushed the market toward capacitive sensors.
- **Insulated, capacitive-only** (common "capacitive soil moisture sensor"
  boards) — the electrodes never galvanically contact the soil solution, so
  they never corrode, but EC/salinity becomes unobservable since there is
  no conduction path.
- **Bare metal, AC-only, duty-cycled** (the approach adopted here) —
  preserves galvanic contact (so EC is observable) while avoiding net
  electrolysis, since AC excitation with no DC bias does not drive a
  sustained one-directional current. Keeping measurement bursts short and
  the probe otherwise unpowered further limits any residual electrochemical
  wear. Electrode material should be corrosion-resistant regardless
  (stainless steel is the practical low-cost choice; gold-plated contacts
  would be a nicer finish for a production version).

This is the reasoning behind choosing bare stainless-steel electrodes over
either a DC resistive probe or an insulated capacitive probe: it is the
only one of the three that keeps both VWC and EC in play from the same two
points of soil contact.

## Reuse of the same two electrodes across modes

Because the excitation is software-controlled, the same physical probe can
be driven differently over time rather than adding more probes:

- A fast, low-power single-frequency read for routine "check-in" moisture
  readings.
- A full multi-frequency sweep, done less often (e.g., once per session or
  on a longer interval), to extract EC and the texture-dispersion signal.

## Front-end hardware choice: AD5933-class impedance analyzer

Building a discrete signal generator + synchronous-detection ADC chain is
possible but adds significant analog design and debugging risk before any
real data exists. The AD5933 (and similar impedance-analyzer front-end
ICs) integrates a DDS excitation source, the analog front end, and an
onboard DFT that returns real/imaginary impedance components directly over
I2C — trading a few dollars of BOM cost for a large reduction in
first-prototype risk. See [`hardware/bom.md`](hardware/bom.md) for the
specific breakout chosen and [`hardware/wiring.md`](hardware/wiring.md) for
how it connects to the probe and the ESP32.

## Precedent

Commercial multi-sensor plant probes (e.g., the well-known
Bluetooth-broadcast plant sensors sold for houseplants) already combine a
two-prong EC/moisture measurement, a light sensor, and a temperature
sensor into one BLE-broadcast device — evidence this class of product is
viable at consumer price points. Most of those, as far as public teardown
information suggests, take a single-frequency reading rather than a full
sweep, meaning they leave the texture-dispersion signal and a rigorously
separated VWC/EC decomposition on the table. The sweep-based approach here
is a deliberate attempt to extract more from the same two points of
contact.

## Open items / next research steps

- Reference dielectric mixing models to consult once real calibration data
  exists: the Topp equation (empirical VWC-vs-permittivity relationship for
  mineral soils) and the Hilhorst model (relates bulk dielectric constant,
  water content, and pore-water conductivity). These will inform the
  calibration fitting in [`analysis/calibration.py`](../analysis/calibration.py).
- Calibration will need reference soil samples spanning known water
  content and known salinity to fit VWC/EC coefficients empirically — see
  the calibration procedure notes in `analysis/calibration.py`.
- Texture-signal separation (Cole-Cole fitting) is deferred until basic
  VWC/EC extraction is validated against reference samples.
