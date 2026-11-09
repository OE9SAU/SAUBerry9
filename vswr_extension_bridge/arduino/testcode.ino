#include <Wire.h>
#include <math.h>

#define ADC_ADDR 0x34
#define VREF 5.0f

#define SHUNT 0.04f           // Shunt-Widerstand 0,04 Ohm
#define GAIN 50.0f            // INA199A1: 50-fache Verstärkung
#define CURRENT_CAL 1.25f     // Kalibrierfaktor

// Vorlaeufige HF-Kalibrierung
#define FWD_CAL 1.0f
#define REV_CAL 1.0f

// ADC-Kanaele
#define CH_REV  0
#define CH_TEMP 1
#define CH_IPA  2
#define CH_FWD  3

// MAX11612 Setup:
// VDD-Referenz, interne Clock, unipolar
// AIN3 bleibt analoger Eingang
#define ADC_SETUP 0x82

uint16_t readADC(uint8_t ch) {

  if (ch > 3) return 0xFFFF;

  // SCAN=11, Single-Ended=1
  // CS=Kanalnummer
  uint8_t config = 0x61 | (ch << 1);

  Wire.beginTransmission(ADC_ADDR);
  Wire.write(ADC_SETUP);
  Wire.write(config);

  if (Wire.endTransmission() != 0)
    return 0xFFFF;

  delayMicroseconds(100);

  if (Wire.requestFrom((uint8_t)ADC_ADDR,
                       (uint8_t)2) != 2)
    return 0xFFFF;

  uint8_t high = Wire.read();
  uint8_t low  = Wire.read();

  return ((uint16_t)(high & 0x0F) << 8) | low;
}

float adcVoltage(uint16_t raw) {
  return raw * VREF / 4096.0f;
}

void setup() {

  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(100000);

  Serial.println();
  Serial.println("VSWR Monitor v1.1");
  Serial.println("ADC: MAX11612");
  Serial.println("I2C: 0x34");
  Serial.println("Referenz: VDD 5V");
  Serial.println("------------------------");
}

void loop() {

  uint16_t rawREV  = readADC(CH_REV);
  uint16_t rawTEMP = readADC(CH_TEMP);
  uint16_t rawIPA  = readADC(CH_IPA);
  uint16_t rawFWD  = readADC(CH_FWD);

  if (rawREV == 0xFFFF ||
      rawTEMP == 0xFFFF ||
      rawIPA == 0xFFFF ||
      rawFWD == 0xFFFF) {

    Serial.println("I2C Lesefehler!");
    delay(3000);
    return;
  }

  float rev  = adcVoltage(rawREV);
  float temp = adcVoltage(rawTEMP);
  // MCP9700: 500 mV bei 0 C, 10 mV pro C
  float temperature = (temp - 0.500f) * 100.0f;
  float ipa  = adcVoltage(rawIPA);
  float fwd  = adcVoltage(rawFWD);

  // PA-Strom
  float current = (ipa / (SHUNT * GAIN)) * CURRENT_CAL;

  // Relative HF-Leistung (noch nicht Watt)
  float pfwd = fwd * fwd * FWD_CAL;
  float prev = rev * rev * REV_CAL;

  // SWR - vorlaeufige Naeherung
  float swr = 0.0f;
  bool swrValid = false;

  if (pfwd > 0.001f) {

    float rho = sqrt(prev / pfwd);

    if (rho >= 1.0f)
      swr = 99.9f;
    else
      swr = (1.0f + rho) / (1.0f - rho);

    swrValid = true;
  }

  Serial.println();
  Serial.println("------------------------");

  Serial.print("REV  AIN0: ");
  Serial.print(rawREV);
  Serial.print("  ");
  Serial.print(rev, 3);
  Serial.println(" V");

  Serial.print("TEMP AIN1: ");
  Serial.print(rawTEMP);
  Serial.print("  ");
  Serial.print(temp, 3);
  Serial.print(" V  |  ");
  Serial.print(temperature, 1);
  Serial.println(" °C");

  Serial.print("IPA  AIN2: ");
  Serial.print(rawIPA);
  Serial.print("  ");
  Serial.print(ipa, 3);
  Serial.println(" V");

  Serial.print("FWD  AIN3: ");
  Serial.print(rawFWD);
  Serial.print("  ");
  Serial.print(fwd, 3);
  Serial.println(" V");

  Serial.println();

  Serial.print("PA-Strom: ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.print("SWR: ");
  if (swrValid)
    Serial.println(swr, 2);
  else
    Serial.println("---");

  Serial.print("P-FWD relativ: ");
  Serial.println(pfwd, 3);

  Serial.print("P-REV relativ: ");
  Serial.println(prev, 3);

  delay(3000);
}
