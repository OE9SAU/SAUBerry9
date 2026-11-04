// filters-interface-radioberry oe9sau
// v2.0: 
// Wahrheitstabelle BPF für Eigenbau SDR-PRE-AMP Filter angepasst, bpf_pin4 = 12 reserve
// Frequenzabhängige Filterumschaltung mittels Frequenz
//

#include <Wire.h>
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
const int FILTER_10m = 232;
const int FILTER_6m = 232;
int currentCW = 0;
boolean genericMode = false;

#define debug

//Pin definitions
#define lpf_board
#if defined lpf_board
int lpf_15_10m = 5;
int lpf_20_17m = 4;
int lpf_40m = 3;
int lpf_80m = 2;
#endif

// Eigenbau SDR-PRE-AMP Filter verwenden
#define bpf_board
#if defined bpf_board
int bpf_pin1 = 9;
int bpf_pin2 = 10;
int bpf_pin3 = 11;
#endif

int ext_pin4 = 12;                 // RESERVE
int ptt_pin = 8;
int tx_pin = 7;
int pa_pin = 6;

int currentBand = 0;
boolean transmit = 0;
boolean transmitAllowed = true;

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

  //Start i2c as slave
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  pinMode(ext_pin4, OUTPUT);      // RESERVE
  digitalWrite(ext_pin4, LOW);    // RESERVE
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

void processFrequency(const int freqHigh[8]) {

  uint32_t frequency = 0;

  // Acht empfangene Bytes als Dezimalziffern interpretieren
  for (int i = 0; i < 8; i++) {
    if (freqHigh[i] < 0 || freqHigh[i] > 9) {
      return;
    }
    frequency = frequency * 10UL + freqHigh[i];
  }

#if defined debug
  Serial.print("Frequency: ");
  Serial.print(frequency);
  Serial.println(" Hz");
#endif

  int bandCommand = 0;

  if (frequency >= 1800000UL && frequency <= 2000000UL) {
    bandCommand = FILTER_160M;
  }
  else if (frequency >= 3500000UL && frequency <= 4000000UL) {
    bandCommand = FILTER_80m;
  }
  else if (frequency >= 5351000UL && frequency <= 5366500UL) {
    bandCommand = FILTER_60m;
  }
  else if (frequency >= 7000000UL && frequency <= 7300000UL) {
    bandCommand = FILTER_40m;
  }
  else if (frequency >= 10100000UL && frequency <= 10150000UL) {
    bandCommand = FILTER_30m;
  }
  else if (frequency >= 14000000UL && frequency <= 14350000UL) {
    bandCommand = FILTER_20m;
  }
  else if (frequency >= 18068000UL && frequency <= 18168000UL) {
    bandCommand = FILTER_17m;
  }
  else if (frequency >= 21000000UL && frequency <= 21450000UL) {
    bandCommand = FILTER_15m;
  }
  else if (frequency >= 24890000UL && frequency <= 24990000UL) {
    bandCommand = FILTER_12m;
  }
  else if (frequency >= 28000000UL && frequency <= 29700000UL) {
    bandCommand = FILTER_10m;
  }
  else if (frequency >= 50000000UL && frequency <= 54000000UL) {
    bandCommand = FILTER_6m;
  }

  if (bandCommand != 0) {
    processCommand(bandCommand);
  }
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
        digitalWrite(bpf_pin1, HIGH);
        digitalWrite(bpf_pin2, LOW);
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
