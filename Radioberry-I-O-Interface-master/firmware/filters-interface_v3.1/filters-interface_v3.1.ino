// v2.01
// Angepasst für SDR-PRE-AMP an den BPF Ausgängen
// Der PTT-Befehl vom I²C-Bus soll Pin 6(pa_pin) erst nach XXXms einschalten.
// Pin 7(tx_pin) schaltet sofort, bei wechsel auf RX fallen beide sofort ab

// v3.0
// Vier Tasternetzwerk an A0 mit Entprellung, Zustandspeicher im EEPROM und Debug Modus

// V3.1
// Erweiterung 8-Port Schieberegister 74HC595 mit A1-A3
//

#include <Wire.h>
#include <EEPROM.h>
#include <stdio.h>

const char SOFTWARE_VERSION[] = "3.0";
const uint8_t I2C_ADDRESS = 0x22; // Generic Board Protocol
uint8_t received;
uint8_t message;
const uint8_t COMMAND_ON_OFF = 0x01;
const uint8_t COMMAND_SPEED = 0x02;

const int FILTER_160M = 1608;
const int FILTER_80m = 1604;
const int FILTER_60m = 1602;
const int FILTER_40m = 802;
const int FILTER_30m = 401;
const int FILTER_20m = 101;
const int FILTER_17m = 164;
const int FILTER_15m = 264;
const int FILTER_12m = 232;
const int FILTER_10m = 209;
//const int FILTER_6m = 232;

int currentCW = 0;
boolean genericMode = false;

//#define debug
#define TASTER_MONITOR 1

// Pin definitions
#define lpf_board
#if defined lpf_board
int lpf_15_10m = 5;
int lpf_20_17m = 4;
int lpf_40m = 3;
int lpf_80m = 2;
#endif

#define bpf_board
#if defined bpf_board
int bpf_pin1 = 9;     // Band S0
int bpf_pin2 = 10;    // Band S1
int bpf_pin3 = 11;    // Band S2
#endif

int ext_pin4 = 12;
int ptt_pin = 8;
int tx_pin = 7;
int pa_pin = 6;

int currentBand = 0;
boolean transmit = 0;
boolean transmitAllowed = true;

const unsigned long PA_DELAY_MS = 100;
volatile bool paPending = false;
volatile unsigned long paStartTime = 0;

void requestEvent();
void receiveEvent(int bytes);
void processPTT(int command);
void processFrequency(int freqHigh[]);
void processCommand(int command);

// ---------- A0-Taster, EEPROM und Monitor ----------

// A0 über 10 kOhm an +5 V.
// Je Taster ein eigener Zweig von A0 nach GND:
// T1: 1 kOhm, T2: 2,2 kOhm, T3: 4,7 kOhm, T4: 10 kOhm.
const uint8_t TASTER_PIN = A0;
const uint8_t TASTER_ANZAHL = 4;
const int TASTER_ADC_MITTE[TASTER_ANZAHL] = {985, 932, 786, 0};
//const int TASTER_ADC_MITTE[TASTER_ANZAHL] = {0, 254, 512, 785}; für original Panel 1 kΩ als Pull-up,0Ω, 330Ω, 1kΩ und 3,3kΩ
const int TASTER_ADC_TOLERANZ = 15;
const int TASTER_ADC_LOSGELASSEN = 1010;
const unsigned long TASTER_ENTPRELLZEIT_MS = 50;

const int8_t TASTER_LOSGELASSEN = -1;
const int8_t TASTER_UNGUELTIG = -2;

const int TASTER_EEPROM_KENNUNG_ADDR = 0;
const int TASTER_EEPROM_ZUSTAND_ADDR = 1;
const uint8_t TASTER_EEPROM_KENNUNG = 0xA7;
const uint8_t TASTER_ZUSTANDSMASKE = 0x0F;

uint8_t tasterZustand = 0;
const uint8_t HC595_DATEN = A1;  // SER, Pin 14
const uint8_t HC595_TAKT  = A2;  // SRCLK, Pin 11
const uint8_t HC595_LATCH = A3;  // RCLK, Pin 12

void tasterAusgaengeAktualisieren() {
  //const uint8_t ausgabe = (tasterZustand & 0x0F) << 4;  // nur Bank2
  //const uint8_t ausgabe = tasterZustand & 0x0F; // nur Bank1

  //beide Banke
  const uint8_t tasterBits = tasterZustand & 0x0F;
  const uint8_t ausgabe = tasterBits | (tasterBits << 4);
  
  digitalWrite(HC595_LATCH, LOW);
  digitalWrite(HC595_TAKT, LOW);
  shiftOut(HC595_DATEN, HC595_TAKT, MSBFIRST, ausgabe);
  digitalWrite(HC595_LATCH, HIGH);

#if TASTER_MONITOR
  Serial.print(F("74HC595 HEX=0x"));
  Serial.println(ausgabe, HEX);
#endif
}

bool tasterBereit = false;
int8_t tasterLetzteMessung = TASTER_UNGUELTIG;
int8_t tasterStabil = TASTER_UNGUELTIG;
unsigned long tasterLetzteAenderung = 0;

#if TASTER_MONITOR
const uint16_t TASTER_ADC_REFERENZ_MV = 5000; // Gemessene AVcc eintragen.
const unsigned long TASTER_MONITOR_INTERVALL_MS = 250;
unsigned long tasterLetzteAusgabe = 0;
#endif

// Tastennummer 1 bis 4.
bool tasterIstEin(uint8_t nummer) {
  return nummer >= 1 && nummer <= TASTER_ANZAHL &&
         (tasterZustand & (1 << (nummer - 1))) != 0;
}

void tasterSetup() {
  pinMode(TASTER_PIN, INPUT);

  if (EEPROM.read(TASTER_EEPROM_KENNUNG_ADDR) == TASTER_EEPROM_KENNUNG) {
    tasterZustand = EEPROM.read(TASTER_EEPROM_ZUSTAND_ADDR) &
                    TASTER_ZUSTANDSMASKE;
  } else {
    EEPROM.update(TASTER_EEPROM_ZUSTAND_ADDR, tasterZustand);
    EEPROM.update(TASTER_EEPROM_KENNUNG_ADDR, TASTER_EEPROM_KENNUNG);
  }

#if TASTER_MONITOR && !defined(debug)
  Serial.begin(115200);
#endif
}

int8_t tasterErkennen(int adc) {
  if (adc >= TASTER_ADC_LOSGELASSEN) {
    return TASTER_LOSGELASSEN;
  }

  for (uint8_t i = 0; i < TASTER_ANZAHL; ++i) {
    if (abs(adc - TASTER_ADC_MITTE[i]) <= TASTER_ADC_TOLERANZ) {
      return i;
    }
  }

  return TASTER_UNGUELTIG;
}

#if TASTER_MONITOR
void tasterMonitorAusgeben(int adc, int8_t taste) {
  const unsigned int millivolt =
      ((unsigned long)adc * TASTER_ADC_REFERENZ_MV + 511) / 1023;

  char anzeige = '?';
  if (taste >= 0) {
    anzeige = '1' + taste;
  } else if (taste == TASTER_LOSGELASSEN) {
    anzeige = '-';
  }

  char zeile[60];
  const int laenge = snprintf(
      zeile, sizeof(zeile),
      "A0 ADC=%d %u mV Taste=%c T1=%d T2=%d T3=%d T4=%d\r\n",
      adc, millivolt, anzeige,
      tasterIstEin(1), tasterIstEin(2), tasterIstEin(3), tasterIstEin(4));

  // Nur ausgeben, wenn die ganze Zeile in den Sendepuffer passt.
  if (laenge > 0 && laenge < (int)sizeof(zeile) &&
      Serial.availableForWrite() >= laenge) {
    Serial.write((const uint8_t *)zeile, (size_t)laenge);
  }
}
#endif

void tasterLoop() {
  const int adc = analogRead(TASTER_PIN);
  const int8_t taste = tasterErkennen(adc);
  const unsigned long jetzt = millis();

  if (taste != tasterLetzteMessung) {
    tasterLetzteMessung = taste;
    tasterLetzteAenderung = jetzt;
  }

  if (taste != tasterStabil &&
      jetzt - tasterLetzteAenderung >= TASTER_ENTPRELLZEIT_MS) {
    tasterStabil = taste;

    if (taste == TASTER_LOSGELASSEN) {
      tasterBereit = true;
    } else if (taste >= 0 && tasterBereit) {
    tasterBereit = false;
    tasterZustand ^= (1U << taste);
    tasterAusgaengeAktualisieren();
    EEPROM.update(TASTER_EEPROM_ZUSTAND_ADDR, tasterZustand);     
  }
}

#if TASTER_MONITOR
  if (jetzt - tasterLetzteAusgabe >= TASTER_MONITOR_INTERVALL_MS) {
    tasterLetzteAusgabe = jetzt;
    tasterMonitorAusgeben(adc, taste);
  }
#endif
}

void setup() {

#if defined lpf_board
  pinMode(lpf_15_10m, OUTPUT);
  pinMode(lpf_20_17m, OUTPUT);
  pinMode(lpf_40m, OUTPUT);
  pinMode(lpf_80m, OUTPUT);
  digitalWrite(lpf_15_10m, LOW);
  digitalWrite(lpf_20_17m, LOW);
  digitalWrite(lpf_40m, LOW);
  digitalWrite(lpf_80m, LOW);
#endif

#if defined bpf_board
  pinMode(bpf_pin1, OUTPUT);
  pinMode(bpf_pin2, OUTPUT);
  pinMode(bpf_pin3, OUTPUT);

  digitalWrite(bpf_pin1, LOW);
  digitalWrite(bpf_pin2, LOW);
  digitalWrite(bpf_pin3, LOW);
#endif

  // Start i2c as slave
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  digitalWrite(ext_pin4, LOW);
  pinMode(ext_pin4, OUTPUT);
  pinMode(ptt_pin, INPUT);
  pinMode(tx_pin, OUTPUT);
  digitalWrite(tx_pin, LOW);
  pinMode(pa_pin, OUTPUT);
  digitalWrite(pa_pin, LOW);

#if defined debug
  Serial.begin(115200);
  Serial.println();
  Serial.print(F("BPF/PTT Steuerung - Software v"));
  Serial.println(SOFTWARE_VERSION);
#endif

  tasterSetup();

digitalWrite(HC595_TAKT, LOW);
digitalWrite(HC595_LATCH, LOW);

pinMode(HC595_DATEN, OUTPUT);
pinMode(HC595_TAKT, OUTPUT);
pinMode(HC595_LATCH, OUTPUT);

tasterAusgaengeAktualisieren();  // EEPROM-Zustände beim Start ausgeben
}

void requestEvent()
{
  if (received == COMMAND_ON_OFF) {

  } else {
    Wire.write(0);
  }
}

int started = 0;

void loop() {
  if (currentCW == 1) {
    processPTT(digitalRead(ptt_pin));
  }

  noInterrupts();

  if (paPending &&
      (unsigned long)(millis() - paStartTime) >= PA_DELAY_MS) {
    digitalWrite(pa_pin, HIGH);
    paPending = false;
  }

  interrupts();

  tasterLoop();
}

void toggleTxmit(int ptt) {
  if (transmitAllowed == true) {
    transmit = ptt;
    if (ptt == 1) {
      digitalWrite(tx_pin, HIGH);
      digitalWrite(pa_pin, HIGH);
    } else {
      digitalWrite(tx_pin, LOW);
      digitalWrite(pa_pin, HIGH);
    }
  }
}

void receiveEvent(int bytes) {

  int byteCount = 0;
  int command = 0;
  uint8_t byte1 = 0;
  uint8_t byte2 = 0;
  uint8_t byte3 = 0;
  int freqHigh [8] = {0, 0, 0, 0, 0, 0, 0, 0};
  while (0 < Wire.available()) {
    byte x = Wire.read();
    if (genericMode == false) {
      if (byteCount == 0) {
        byte1 = x;
      }
      if (byteCount == 1) {
        byte2 = x;
        command = command + (byte2 * 100);
      }
      if (byteCount == 2) {
        byte3 = x;
        command = command + byte3;
      }
      if (byte1 == 2 && byte2 == 2 && byte3 == 3) {
        genericMode = true;
        byte1 = 0;
        //#if defined debug
        //        Serial.println("Generic Mode set to true");
        //#endif
      }
      byteCount = byteCount + 1;
    } else {

      if (byteCount == 0) {
        byte1 = x;
      }
      if (byteCount == 1) {
        byte2 = x;
        command = command + (byte2 * 100);
      }
      if (byteCount == 2) {
        byte3 = x;
        command = command + byte3;
      }

      if (byteCount > 0) {
        freqHigh[byteCount - 1] = x;
      }

      byteCount = byteCount + 1;
    }
  }

#if defined debug
  Serial.print("Command Status: ");
  Serial.println(command);
  Serial.print("Byte1 Status: ");
  Serial.println(byte1);
  Serial.print("Byte2 Status: ");
  Serial.println(byte2);
  Serial.print("Byte3 Status: ");
  Serial.println(byte3);
  Serial.print("freq Received: ");
  Serial.print(freqHigh[0]);
  Serial.print(freqHigh[1]);
  Serial.print(freqHigh[2]);
  Serial.print(freqHigh[3]);
  Serial.print(freqHigh[4]);
  Serial.print(freqHigh[5]);
  Serial.print(freqHigh[6]);
  Serial.println(freqHigh[7]);
#endif
  int pttTrig = digitalRead(ptt_pin);
  if (byte1 == 3 && pttTrig == HIGH && byte2 > 0) {
    currentCW = byte3;
    processPTT(byte2);
    command = 0;
  } else if (byte1 == 3 && pttTrig == HIGH && byte3 > 0) {
    currentCW = byte3;
    processPTT(byte3);
    command = 0;
  } else if (byte1 == 3) {
    currentCW = byte3;
    processPTT(0);

  } else {
    currentCW = 0;
    processPTT(0);
  }
  if (byte1 == 4) {
    processFrequency(freqHigh);
    command = 0;

  }
  if (command != 0) {
#if defined debug
    Serial.print("Command Sended: ");
    Serial.println(command);
#endif
    processCommand(command);
  }

}

void processPTT(int command) {
  if (command != 0) {
    if (digitalRead(tx_pin) == LOW) {
      digitalWrite(tx_pin, HIGH);
      paStartTime = millis();
      paPending = true;
    }
  } else {
    paPending = false;
    digitalWrite(pa_pin, LOW);
    digitalWrite(tx_pin, LOW);
  }
}

void processFrequency(int freqHigh[]) {

}

void processCommand(int command) {
  started =  1;
  if (currentBand != command) {
    currentBand = command;
    switch (command) {
      case FILTER_160M:
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, HIGH);
#endif
#if defined lpf_board
        //Not supported
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_80m:
#if defined bpf_board
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, HIGH);
#endif
        break;
      case FILTER_60m:
#if defined bpf_board
        //Not supported
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        //Not supported
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_40m:
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, HIGH);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_30m:
#if defined bpf_board
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        //Not supported
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_20m:
#if defined bpf_board
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, HIGH);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_17m:
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, HIGH);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_15m:
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, HIGH);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      case FILTER_12m:
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, HIGH);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, LOW);
#endif
        break;
      default:
        //TODO Disable TX
        //Wire.write(0);
#if defined bpf_board
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
#endif
#if defined lpf_board
        digitalWrite(lpf_15_10m, LOW);
        digitalWrite(lpf_20_17m, LOW);
        digitalWrite(lpf_40m, LOW);
        digitalWrite(lpf_80m, HIGH);
#endif
        break;
    }
  }
}
