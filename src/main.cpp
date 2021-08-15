
#include <Arduino.h>
#include <USB-MIDI.h>
#include <Adafruit_NeoPixel.h>

//Defines:
#define KNOB_AMOUNT
#define BUTTON_OFFSET 8
#define ANALOG_READ_SAMPLES 15

const uint8_t pots[6] = { A3, A1, A10, A2, A0, A9 };
uint8_t potMemory[6] = { 0,0,0,0,0,0 };
const uint8_t buttons[4] = { 2, 3, 4, 5 };
uint8_t buttonMemory[4] = { 0 ,0, 0, 0 };
bool buttonCommand[4] = { 0,0,0,0 };
uint16_t hue[4] = { 0,0,0,0 };
uint32_t previousmillis;
uint8_t fader = A8;
uint8_t faderMemory;
//Global Variables:
uint8_t oldReading;
uint8_t midiChannel;

#pragma region MIDISTUFF
byte sysex14[] = { 0xF0, 0x43, 0x20, 0x7E, 0x4C, 0x4D, 0x20, 0x20, 0x38, 0x39, 0x37, 0x33, 0x50, 0xF7 };
byte sysex15[] = { 0xF0, 0x43, 0x20, 0x7E, 0x4C, 0x4D, 0x20, 0x20, 0x38, 0x39, 0x37, 0x33, 0x50, 0x4D, 0xF7 };
byte sysex16[] = { 0xF0, 0x43, 0x20, 0x7E, 0x4C, 0x4D, 0x20, 0x20, 0x38, 0x39, 0x37, 0x33, 0x32, 0x50, 0x4D, 0xF7 };
byte sysexBig[] = { 0xF0, 0x41,
                    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
                    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
                    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
                    0x7a,

                    0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
                    0xF7
};

USBMIDI_CREATE_DEFAULT_INSTANCE();
#pragma endregion MIDISTUFF

void OnMidiSysEx(byte *data, unsigned length);
void asyncRainbow(void);
uint8_t analogReadLPF(uint8_t, uint8_t);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, 6, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  strip.begin();
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
  }
  delay(1000);
  //Read button inputs, if there are, and write it as the new MIDI channel:
  if ((!digitalRead(buttons[0])) || (!digitalRead(buttons[1])) || (!digitalRead(buttons[2])) || (!digitalRead(buttons[3]))) {
    uint8_t newMidiChannel = 0;
    for (uint8_t i = 0; i < 4; i++) {
      newMidiChannel |= (!digitalRead(buttons[i]) ? 0x01:0x00) << i;
    }
    delay(1000);
    for (uint8_t i = 0; i < newMidiChannel; i++) {
      strip.setPixelColor(0, 0x00ff00);
      strip.show();
      delay(200);
      strip.setPixelColor(0, 0x000000);
      strip.show();
      delay(200);
    }
    //Write the new channel to the EEPROM:
    eeprom_update_byte(0, newMidiChannel);
    
  }
  midiChannel = eeprom_read_byte(0);
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(pots[i], INPUT);
  }

  pinMode(fader, INPUT);
  // Listen for MIDI messages on channel 1
  MIDI.begin(1);

  MIDI.setHandleSystemExclusive(OnMidiSysEx);

}

void loop() {
  // Listen to incoming notes
  MIDI.read();
  //read all pot values:
  uint8_t potTempMemory[6];
  for (uint8_t i = 0; i < 6; i++) {
    potTempMemory[i] = analogReadLPF(pots[i], ANALOG_READ_SAMPLES); //Read the analog pin, take the average of multiple readings to reduce jitter
    if ((potTempMemory[i] < potMemory[i] - 1) || (potTempMemory[i] > potMemory[i] + 1)) { //Even after analogReadLPF still lot of jitter, remove differences of only 1 might fix it
      Serial.println("Pot " + String(i) + " changed to " + String(potTempMemory[i]));
      MIDI.sendControlChange(i + 1, potTempMemory[i], midiChannel);
      potMemory[i] = potTempMemory[i];  //Write the new value into memory
    }
  }
  uint8_t tempFader = analogRead(fader) >> 3;
  if (tempFader != faderMemory) {
    Serial.println("Fader changed to " + String(tempFader));
    MIDI.sendControlChange(7, tempFader, midiChannel);
    faderMemory = tempFader;
  }

  uint8_t tempButtonState[4];
  for (uint8_t i = 0; i < 4; i++) {
    tempButtonState[i] = digitalRead(buttons[i]);
    if (tempButtonState[i] != buttonMemory[i]) {
      if (tempButtonState[i] == 0) {
        Serial.println("Button " + String(i) + " was pressed");
        //Toggle the MIDI control:
        buttonCommand[i] = (buttonCommand[i] ^ 0X01); // Invert the current state
        MIDI.sendControlChange(BUTTON_OFFSET + i, buttonCommand[i] ? 127 : 0, midiChannel);
        hue[i] = 0;

      }
      else Serial.println("Button " + String(i) + " was Released");
      buttonMemory[i] = tempButtonState[i];
    }
  }
  asyncRainbow();
}

void asyncRainbow(void) {
  if (previousmillis + 50 < millis()) {
    previousmillis = millis();
    for (uint8_t i = 0; i < 4; i++) {
      buttonCommand[i] ? strip.setPixelColor(i, strip.ColorHSV(hue[i])) : strip.setPixelColor(i, 0);
      hue[i] += 80;
    }
  }
  strip.show();
}

uint8_t analogReadLPF(uint8_t pin, uint8_t samples) {
  uint32_t samplesTotal = 0; //Place to store all the samples
  for (uint8_t i = 0; i < samples; i++) {
    samplesTotal += analogRead(pin) >> 3; //Perform analogRead and bit shift to the left to convert from 10 bit to 7 bit
  }
  return (uint8_t)(samplesTotal / samples); //Divide all added samples by the amount of samples to get a filtered return
}

void OnMidiSysEx(byte *data, unsigned length) {
  Serial.print(F("SYSEX: ("));
  Serial.print(length);
  Serial.print(F(" bytes) "));
  for (uint16_t i = 0; i < length; i++) {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}
