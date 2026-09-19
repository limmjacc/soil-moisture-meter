// ESP32 firmware, Phase 1 (breadboard): drives an AD5933-class impedance
// analyzer through a frequency sweep across the soil probe electrodes,
// reads soil temperature (thermistor) and ambient temp/humidity (DHT11),
// and streams one CSV line per sweep point over Serial for capture by
// analysis/capture.py.
//
// AD5933 register map, control-register function codes, and the
// reset/standby/init/sweep/increment sequence below match the AD5933
// datasheet and are consistent across published reference implementations
// with high confidence. The two range/gain bit values flagged below are
// lower-confidence from memory — cross-check against datasheet Table 8/9
// with a logic analyzer during bring-up before trusting non-default
// excitation range or gain settings, and before trusting absolute (as
// opposed to calibration-relative) impedance magnitudes.
//
// Not yet bench-verified against physical hardware.

#include <Wire.h>
#include <DHT.h>

// ---- Pin assignments (see docs/hardware/wiring.md) ----
#define DHTPIN 4
#define DHTTYPE DHT11
#define THERMISTOR_PIN 34

// ---- AD5933 ----
#define AD5933_ADDR 0x0D

#define AD5933_REG_CONTROL_HB   0x80
#define AD5933_REG_CONTROL_LB   0x81
#define AD5933_REG_START_FREQ   0x82  // 3 bytes: 0x82-0x84
#define AD5933_REG_FREQ_INCR    0x85  // 3 bytes: 0x85-0x87
#define AD5933_REG_NUM_INCR     0x88  // 2 bytes: 0x88-0x89
#define AD5933_REG_SETTLING     0x8A  // 2 bytes: 0x8A-0x8B
#define AD5933_REG_STATUS       0x8F
#define AD5933_REG_REAL_DATA    0x94  // 2 bytes, then 0x96-0x97 imaginary (auto-increments on block read)

#define AD5933_CMD_SET_POINTER  0xB0

#define AD5933_FUNC_INIT_START_FREQ 0x10
#define AD5933_FUNC_START_SWEEP     0x20
#define AD5933_FUNC_INCREMENT_FREQ  0x30
#define AD5933_FUNC_STANDBY         0xA0
#define AD5933_RESET_BIT            0x10  // written to control LB register

// Range = 2 Vpp (D10:D9 = 00), PGA gain = x1 (D8 = 1).
// VERIFY against datasheet Table 8/9 before relying on these for absolute
// (non-calibration-relative) measurements.
#define AD5933_RANGE_GAIN_DEFAULT 0x01

#define AD5933_STATUS_DATA_READY     0x02
#define AD5933_STATUS_SWEEP_COMPLETE 0x04

// Nominal internal oscillator frequency. Not laboratory-accurate; fine for
// a calibration-relative measurement approach, per docs/methodology.md.
#define AD5933_MCLK_HZ 16776000UL

// ---- Sweep configuration ----
// 1 kHz to ~99 kHz in 2 kHz steps. Revisit range once real soil data shows
// where the moisture (higher-frequency capacitive) and EC (lower-frequency
// resistive) signals actually separate for the probes in use.
#define SWEEP_START_HZ   1000UL
#define SWEEP_INCR_HZ    2000UL
#define SWEEP_NUM_INCR   49     // 9-bit register, max 511
#define SWEEP_SETTLING_CYCLES 15  // x1 multiplier

// ---- Thermistor (10k NTC, beta=3950) divider: 3.3V - Rfixed - ADC - NTC - GND ----
#define THERMISTOR_R_FIXED 10000.0
#define THERMISTOR_R0 10000.0
#define THERMISTOR_T0_K 298.15
#define THERMISTOR_BETA 3950.0
#define ADC_MAX 4095.0

DHT dht(DHTPIN, DHTTYPE);

void ad5933WriteByte(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AD5933_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void ad5933SetPointer(uint8_t reg) {
  Wire.beginTransmission(AD5933_ADDR);
  Wire.write(AD5933_CMD_SET_POINTER);
  Wire.write(reg);
  Wire.endTransmission();
}

uint8_t ad5933ReadByte(uint8_t reg) {
  ad5933SetPointer(reg);
  Wire.requestFrom(AD5933_ADDR, 1);
  return Wire.read();
}

// Reads `len` bytes starting at `reg`, relying on the AD5933's
// auto-incrementing pointer during a block read (used here to fetch real
// and imaginary data in a single 4-byte transaction).
void ad5933ReadBlock(uint8_t reg, uint8_t *buf, uint8_t len) {
  ad5933SetPointer(reg);
  Wire.requestFrom(AD5933_ADDR, (int)len);
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
}

uint32_t ad5933FreqToCode(uint32_t freqHz) {
  double code = (double)freqHz * 268435456.0 / (double)AD5933_MCLK_HZ;  // 2^27
  return (uint32_t)(code + 0.5) & 0x00FFFFFF;
}

void ad5933WriteFreqRegister(uint8_t startReg, uint32_t code) {
  ad5933WriteByte(startReg,     (code >> 16) & 0xFF);
  ad5933WriteByte(startReg + 1, (code >> 8) & 0xFF);
  ad5933WriteByte(startReg + 2, code & 0xFF);
}

void ad5933Reset() {
  ad5933WriteByte(AD5933_REG_CONTROL_LB, AD5933_RESET_BIT);
}

void ad5933Standby() {
  ad5933WriteByte(AD5933_REG_CONTROL_HB, AD5933_FUNC_STANDBY | AD5933_RANGE_GAIN_DEFAULT);
}

void ad5933ProgramSweep() {
  ad5933Reset();
  ad5933Standby();

  ad5933WriteFreqRegister(AD5933_REG_START_FREQ, ad5933FreqToCode(SWEEP_START_HZ));
  ad5933WriteFreqRegister(AD5933_REG_FREQ_INCR, ad5933FreqToCode(SWEEP_INCR_HZ));

  ad5933WriteByte(AD5933_REG_NUM_INCR, (SWEEP_NUM_INCR >> 8) & 0x01);
  ad5933WriteByte(AD5933_REG_NUM_INCR + 1, SWEEP_NUM_INCR & 0xFF);

  // x1 settling-time multiplier: top bits of the HB settling byte stay 0.
  ad5933WriteByte(AD5933_REG_SETTLING, (SWEEP_SETTLING_CYCLES >> 8) & 0x01);
  ad5933WriteByte(AD5933_REG_SETTLING + 1, SWEEP_SETTLING_CYCLES & 0xFF);

  ad5933WriteByte(AD5933_REG_CONTROL_HB, AD5933_FUNC_INIT_START_FREQ | AD5933_RANGE_GAIN_DEFAULT);
  delay(2);  // allow output to settle at the start frequency before sweeping
}

bool ad5933WaitForStatus(uint8_t mask, uint16_t timeoutMs) {
  uint32_t start = millis();
  while ((ad5933ReadByte(AD5933_REG_STATUS) & mask) == 0) {
    if (millis() - start > timeoutMs) return false;
    delay(1);
  }
  return true;
}

// Runs one full sweep, calling `onPoint(freqHz, real, imag)` for each
// point. Returns false if a step times out (probe disconnected, bad wiring).
bool ad5933RunSweep(void (*onPoint)(uint32_t freqHz, int16_t real, int16_t imag)) {
  ad5933ProgramSweep();

  ad5933WriteByte(AD5933_REG_CONTROL_HB, AD5933_FUNC_START_SWEEP | AD5933_RANGE_GAIN_DEFAULT);

  uint32_t freq = SWEEP_START_HZ;
  for (uint16_t i = 0; i <= SWEEP_NUM_INCR; i++) {
    if (!ad5933WaitForStatus(AD5933_STATUS_DATA_READY, 500)) return false;

    uint8_t buf[4];
    ad5933ReadBlock(AD5933_REG_REAL_DATA, buf, 4);
    int16_t real = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t imag = (int16_t)((buf[2] << 8) | buf[3]);

    onPoint(freq, real, imag);

    if (i < SWEEP_NUM_INCR) {
      ad5933WriteByte(AD5933_REG_CONTROL_HB, AD5933_FUNC_INCREMENT_FREQ | AD5933_RANGE_GAIN_DEFAULT);
      freq += SWEEP_INCR_HZ;
    }
  }

  if (!ad5933WaitForStatus(AD5933_STATUS_SWEEP_COMPLETE, 500)) return false;
  ad5933Standby();
  return true;
}

float readThermistorC() {
  int raw = analogRead(THERMISTOR_PIN);
  float ratio = (float)raw / ADC_MAX;
  if (ratio <= 0.0 || ratio >= 1.0) return NAN;  // open/short divider
  float rTherm = THERMISTOR_R_FIXED * ratio / (1.0 - ratio);
  float tempK = 1.0 / (1.0 / THERMISTOR_T0_K + log(rTherm / THERMISTOR_R0) / THERMISTOR_BETA);
  return tempK - 273.15;
}

// Sweep point handler: prints one CSV row immediately, so a partial sweep
// (e.g. interrupted mid-run) still leaves usable data in the capture log.
void printSweepPoint(uint32_t freqHz, int16_t real, int16_t imag) {
  Serial.print(millis());
  Serial.print(',');
  Serial.print(freqHz);
  Serial.print(',');
  Serial.print(real);
  Serial.print(',');
  Serial.println(imag);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  dht.begin();
  analogReadResolution(12);

  Serial.println("# soil-moisture-meter Phase 1 firmware");
  Serial.println("# sweep_ts_ms,freq_hz,real,imag");
}

void loop() {
  float soilTempC = readThermistorC();
  float airHumidity = dht.readHumidity();
  float airTempC = dht.readTemperature();

  Serial.print("# env,soil_temp_c=");
  Serial.print(soilTempC);
  Serial.print(",air_temp_c=");
  Serial.print(airTempC);
  Serial.print(",air_humidity_pct=");
  Serial.println(airHumidity);

  if (!ad5933RunSweep(printSweepPoint)) {
    Serial.println("# sweep_error: timed out waiting on AD5933 status bit — check wiring/I2C address");
  }

  delay(5000);
}
