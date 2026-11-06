
#include <Wire.h>
#include <math.h>

#define ADC_ADDR 0x34
#define VREF 5.0       // MAX11612 bei 5V
                       // MAX11613: 3.3V verwenden

#define SHUNT 0.04
#define GAIN 50.0

// HF-Kalibrierung (vorlaeufig)
#define FWD_CAL 1.0
#define REV_CAL 1.0

uint16_t readADC(uint8_t ch) {
  if (ch > 3) return 0xFFFF;

  Wire.beginTransmission(ADC_ADDR);

  // VDD als Referenz, interne Clock, unipolar
  Wire.write(0x82);

  // Einzelkanal, Single-Ended
  Wire.write(0x61 | (ch << 1));

  if (Wire.endTransmission() != 0)
    return 0xFFFF;

  if (Wire.requestFrom((uint8_t)ADC_ADDR,
                       (uint8_t)2) != 2)
    return 0xFFFF;

  uint8_t high = Wire.read();
  uint8_t low = Wire.read();

  return ((high & 0x0F) << 8) | low;
}

float readVoltage(uint8_t ch) {
  uint16_t raw = readADC(ch);

  if (raw == 0xFFFF) return NAN;

  return raw * VREF / 4096.0;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  Serial.println("VSWR Monitor v1.0");
  Serial.println("MAX11612/11613");
}

void loop() {
  float rev   = readVoltage(0);
  float temp  = readVoltage(1);
  float vipa  = readVoltage(2);
  float fwd   = readVoltage(3);

  if (isnan(rev) || isnan(temp) ||
      isnan(vipa) || isnan(fwd)) {
    Serial.println("I2C Fehler!");
    delay(500);
    return;
  }

  // Stromberechnung
  float current = vipa / (SHUNT * GAIN);

  // Relative HF-Leistung
  float pfwd = fwd * fwd * FWD_CAL;
  float prev = rev * rev * REV_CAL;

  // SWR
  float swr = 1.0;

  if (pfwd > 0.001) {
    float rho = sqrt(prev / pfwd);

    if (rho >= 1.0)
      swr = 99.9;
    else
      swr = (1.0 + rho) / (1.0 - rho);
  }

  Serial.println("------------------------");

  Serial.print("FWD: ");
  Serial.print(fwd, 3);
  Serial.println(" V");

  Serial.print("REV: ");
  Serial.print(rev, 3);
  Serial.println(" V");

  Serial.print("SWR: ");
  if (pfwd <= 0.001)
    Serial.println("---");
  else
    Serial.println(swr, 2);

  Serial.print("Strom: ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.print("Vtemp: ");
  Serial.print(temp, 3);
  Serial.println(" V");

  Serial.print("P-FWD rel.: ");
  Serial.println(pfwd, 3);

  Serial.print("P-REV rel.: ");
  Serial.println(prev, 3);

  delay(500);
}
