# Breadboard wiring guide

This reflects typical pinouts for a generic ESP32 DevKitC-style board and a
generic AD5933 breakout module. **Verify every pin against the silkscreen
and datasheet of your specific boards before powering anything on** — this
has not been bench-verified against physical hardware, and breakout boards
vary between manufacturers (particularly the AD5933 module's logic-level
and VCC requirements — some breakouts are 5V-only, some are 3.3V-tolerant;
check yours, since the ESP32's I2C pins are not 5V-tolerant).

## ESP32 ↔ AD5933 breakout (I2C)

| ESP32 pin | AD5933 breakout pin | Notes |
|---|---|---|
| 3V3 (or 5V, if the breakout requires it — see caution above) | VCC | Confirm breakout's supply voltage requirement first |
| GND | GND | |
| GPIO21 (default SDA) | SDA | Add external 4.7kΩ pull-up to 3.3V if the breakout has none on board |
| GPIO22 (default SCL) | SCL | Add external 4.7kΩ pull-up to 3.3V if the breakout has none on board |

AD5933 default I2C address is `0x0D`.

## AD5933 ↔ probe electrodes

The AD5933 breakout typically exposes the excitation output and the
receive input as two separate pins/pads (often labeled something like
`VOUT`/`OUT` and `VIN`/`IN`, sometimes via an onboard feedback resistor
network for the current-to-voltage stage). Wire:

- Excitation output pin → one probe electrode
- Receive input pin → the other probe electrode
- Confirm the breakout's onboard feedback resistor (`RFB`) is populated
  with a value appropriate for the expected soil impedance range, per that
  board's documentation — this sets the current-to-voltage gain stage and
  needs to roughly match the impedance magnitude being measured for good
  signal-to-noise.

For calibration runs (see [`../../analysis/calibration.py`](../../analysis/calibration.py)),
temporarily replace the probe electrodes with one of the reference
resistors from the BOM in the same two positions.

## Thermistor (soil temperature)

Standard NTC voltage-divider: divider midpoint to an ESP32 ADC-capable
GPIO (e.g., GPIO34, input-only, no internal pull-up conflicts).

| Connection | Notes |
|---|---|
| 3.3V → 10kΩ fixed resistor → ADC pin (GPIO34) | Divider top half |
| ADC pin (GPIO34) → 10kΩ NTC thermistor → GND | Divider bottom half |

Mount the thermistor bead at the probe tip, physically alongside (not
touching) the two electrodes, so it reads soil temperature rather than
ambient air temperature.

## DHT11 (ambient temperature/humidity)

Reuses the same wiring as the original display prototype:

| ESP32 pin | DHT11 pin |
|---|---|
| 3.3V | VCC |
| GND | GND |
| GPIO4 (any free digital GPIO) | DATA |

Add a 10kΩ pull-up resistor from DATA to VCC if the DHT11 breakout does not
already include one on board.

## Bring-up order

1. ESP32 + AD5933 + one known reference resistor (not the probe) — validate
   I2C communication and a basic frequency sweep before introducing any
   soil variable.
2. Add the thermistor and DHT11 — validate both read plausible values.
3. Swap the reference resistor for the actual probe electrodes, dry (in
   air) — confirm a stable, sane high-impedance reading.
4. Insert the probe into a soil sample — begin real calibration runs per
   [`../methodology.md`](../methodology.md).
