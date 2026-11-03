//filters-interface_v2-1.ino
// Anpassungen OE9SAU
// Tasternetzwerk mit Ausgängen udn EEPROM Speicherung

#include <Wire.h>
#include <EEPROM.h>
//const uint8_t I2C_ADDRESS = 0x21; //Alex Board Protocol
const uint8_t I2C_ADDRESS = 0x22; //Generic Board Protocol
uint8_t received;
uint8_t message;
const uint8_t COMMAND_ON_OFF = 0x01;
const uint8_t COMMAND_SPEED = 0x02;

const int FILTER_160M = 8;
const int FILTER_80m = 4;
const int FILTER_60m = 2;
const int FILTER_40m = 802;
const int FILTER_30m = 401;
const int FILTER_20m = 101;
const int FILTER_17m = 164;
const int FILTER_15m = 264;
const int FILTER_12m = 232;
const int FILTER_10m = 232; // gleiches Kommando wie 12m
const int FILTER_6m = 232;  // gleiches Kommando wie 12m; NICHT unterscheidbar
int currentCW = 0;
boolean genericMode = false;

#define debug

//Pin definitions
// Neues SDR Pre-Amp Filterboard (74CBT3251)
// D2=S0, D3=S1, D4=S2, D5=Reserve
const uint8_t filter_s0 = 2;
const uint8_t filter_s1 = 3;
const uint8_t filter_s2 = 4;

// Kanal 0..4 = Filter; Kanal 7 = GND (Fallback)
void selectSDRFilter(uint8_t channel) {
  digitalWrite(filter_s0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(filter_s1, (channel & 0x02) ? HIGH : LOW);
  digitalWrite(filter_s2, (channel & 0x04) ? HIGH : LOW);
}

#define bpf_board_russian
//https://www.ebay.ca/itm/HF-BPF-band-pass-filter-RTL-SDR-HERMES-ODYSSEY-HiQSDR-Red-Pitaya-transceiver/332074659425?ssPageName=STRK%3AMEBIDX%3AIT&_trksid=p2057872.m2749.l2649
//BPF pins logic definition russian board
//160m 1000
//80m 0100
//40m 1100
//30m 0010
//20m 1010
//17m 0110
//15m 1110
//12m 0001
//10m 1001
#if defined bpf_board_russian
int bpf_pin1 = 9;
int bpf_pin2 = 10;
int bpf_pin3 = 11;
int bpf_pin4 = 12;
#endif

// Vier Taster an A0 (10k Pull-up nach +5V).
// Je Taster: A0 -- Widerstand -- Taster -- GND
// T1=1k, T2=2.2k, T3=4.7k, T4=10k.
// Nur einzelne Tastendruecke werden ausgewertet.
const uint8_t buttonAnalogPin = A0;
const uint8_t toggleOutputPins[4] = {A1, A2, A3, 5};
const uint16_t buttonAdcCenters[4] = {93, 184, 327, 512};
const uint8_t EEPROM_MAGIC_ADDR = 0;
const uint8_t EEPROM_STATE_ADDR = 1;
const uint8_t EEPROM_MAGIC = 0xA7;
const unsigned long debounceMs = 50;
uint8_t toggleStates = 0;
int8_t lastRawButton = -1;
int8_t stableButton = -1;
unsigned long buttonChangeMs = 0;
bool buttonArmed = true;

// Werte mit Sicherheitsabstand klassifizieren; sonst keine Taste.
int8_t readButton() {
  const int adc = analogRead(buttonAnalogPin);
  for (uint8_t i = 0; i < 4; ++i) {
    if (abs(adc - (int)buttonAdcCenters[i]) <= 35) return i;
  }
  return -1;
}

void setToggleOutputs() {
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(toggleOutputPins[i], (toggleStates & (1 << i)) ? HIGH : LOW);
  }
}

void setupToggleOutputs() {
  // Vor Aktivierung der Pins die gespeicherten Ausgangspegel setzen.
  if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC) {
    toggleStates = EEPROM.read(EEPROM_STATE_ADDR) & 0x0F;
  } else {
    toggleStates = 0;
    EEPROM.update(EEPROM_STATE_ADDR, toggleStates);
    EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(toggleOutputPins[i], (toggleStates & (1 << i)) ? HIGH : LOW);
    pinMode(toggleOutputPins[i], OUTPUT);
  }
  pinMode(buttonAnalogPin, INPUT); // externer 10k Pull-up, kein INPUT_PULLUP
}

void pollToggleButtons() {
  const int8_t raw = readButton();
  const unsigned long now = millis();
  if (raw != lastRawButton) {
    lastRawButton = raw;
    buttonChangeMs = now;
  }
  if ((now - buttonChangeMs) < debounceMs || raw == stableButton) return;
  stableButton = raw;
  if (stableButton == -1) {
    buttonArmed = true;
  } else if (buttonArmed) {
    buttonArmed = false;
    toggleStates ^= (1 << stableButton);
    setToggleOutputs();
    EEPROM.update(EEPROM_STATE_ADDR, toggleStates);
#if defined debug
    Serial.print("Toggle output ");
    Serial.print(stableButton + 1);
    Serial.print(" = ");
    Serial.println((toggleStates & (1 << stableButton)) ? "ON" : "OFF");
#endif
  }
}

int ptt_pin = 8;
int tx_pin = 7;
int pa_pin = 6;

int currentBand = -1;
boolean transmit = 0;
boolean transmitAllowed = true;

void setup() {
  setupToggleOutputs();

  // Erst GND-Kanal wählen, dann Pins als Ausgänge aktivieren.
  digitalWrite(filter_s0, HIGH);
  digitalWrite(filter_s1, HIGH);
  digitalWrite(filter_s2, HIGH);
  pinMode(filter_s0, OUTPUT);
  pinMode(filter_s1, OUTPUT);
  pinMode(filter_s2, OUTPUT);

#if defined bpf_board_russian
  pinMode(bpf_pin1, OUTPUT);
  pinMode(bpf_pin2, OUTPUT);
  pinMode(bpf_pin3, OUTPUT);
  pinMode(bpf_pin4, OUTPUT);
  digitalWrite(bpf_pin1, LOW);
  digitalWrite(bpf_pin2, LOW);
  digitalWrite(bpf_pin3, LOW);
  digitalWrite(bpf_pin4, LOW);
#endif

  //Start i2c as slave
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  pinMode(ptt_pin, INPUT);
  pinMode(tx_pin, OUTPUT);
  digitalWrite(tx_pin, LOW);
  pinMode(pa_pin, OUTPUT);
  digitalWrite(pa_pin, LOW);

#if defined debug
  Serial.begin(115200);
#endif
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
  pollToggleButtons();

  int ptt = digitalRead(ptt_pin);
  if (ptt == HIGH && currentCW == 1) {
    processPTT(ptt);
  } else if (ptt == LOW && currentCW == 1) {
    processPTT(ptt);
  }
  delay(50);
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
  digitalWrite(tx_pin, command);
  digitalWrite(pa_pin, command);
}
void processFrequency(int *command) {

}
void processCommand(int command) {
  started =  1;
  if (currentBand != command) {
    currentBand = command;
    switch (command) {
      case FILTER_160M:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(4); // 74CBT3251

        break;
      case FILTER_80m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(3); // 74CBT3251

        break;
      case FILTER_60m:
#if defined bpf_board_russian
        //Not supported
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(2); // 74CBT3251

        break;
      case FILTER_40m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(2); // 74CBT3251

        break;
      case FILTER_30m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, HIGH);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(1); // 74CBT3251

        break;
      case FILTER_20m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, HIGH);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(1); // 74CBT3251

        break;
      case FILTER_17m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, HIGH);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(0); // 74CBT3251

        break;
      case FILTER_15m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, HIGH);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(0); // 74CBT3251

        break;
      case FILTER_12m:
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, LOW);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, HIGH);
#endif
        selectSDRFilter(0); // 74CBT3251

        break;
      // 10m/6m share command 232 with 12m. 6m unsupported.
      default:
        //TODO Disable TX
        //Wire.write(0);
#if defined bpf_board_russian
        digitalWrite(bpf_pin1, LOW);
        digitalWrite(bpf_pin2, HIGH);
        digitalWrite(bpf_pin3, LOW);
        digitalWrite(bpf_pin4, LOW);
#endif
        selectSDRFilter(7); // 74CBT3251

        break;
    }
  }
}