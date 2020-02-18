#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Keypad.h>

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=419,449
AudioSynthWaveformSine   sine2;          //xy=420,501
AudioPlayMemory          playMem1;       //xy=426,602
AudioInputI2S            i2s1;           //xy=432,352
AudioMixer4              mixer1;         //xy=663,419
AudioAnalyzeRMS          rms1;           //xy=673,242
AudioOutputI2S           i2s2;           //xy=860,343
AudioConnection          patchCord1(sine1, 0, mixer1, 1);
AudioConnection          patchCord2(sine2, 0, mixer1, 2);
AudioConnection          patchCord3(playMem1, 0, mixer1, 3);
AudioConnection          patchCord4(i2s1, 0, mixer1, 0);
AudioConnection          patchCord5(i2s1, 0, rms1, 0);
AudioConnection          patchCord6(mixer1, 0, i2s2, 0);
AudioConnection          patchCord7(mixer1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=657,653
// GUItool: end automatically generated code

float yellThreshold = 0.35;  // how loud somebody has to yell to trigger the yell send
float toneVol = 0.7; // amplitude of the beeps, varies depending on phone handset resistance

unsigned long prevMillis1 = 0;
unsigned long prevMillis2 = 0;
const long interval1 = 300; //used for the RMS delay to avoid multiple yell readings
const long interval2 = 200; //length of tone

bool dialToneOn = false;
bool busySignalOn = false;

int receiverState; //tracking variable for the hangup/pickup situation

//I want to send Function keys here, so we're using DEC values which will convert
//to ASCII when called by the Keyboard.write() function.
char pickup [] = {'0', '-', '='};
char hangup [] = {'p', '[', ']'};
char yell [] = {';', '/', '`'};


const int hangupPin = 10; // the pin triggered by the hangup switch (beware Teensy audio pins!)

const byte ROWS = 4; //4 row 3 column- standard ATT phone layout
const byte COLS = 3;
byte keys[ROWS][COLS] = {
  {1, 2, 3},
  {4, 5, 6},
  {7, 8, 9},
  {10, 11, 12},
};

const int playerNumber = 0; //set 0 for P1, 1 for P2, 2 for P3

//blue phone
byte rowPins[ROWS] = {5, 6, 7, 8}; //
byte colPins[COLS] = {4, 3, 2}; //

//yellow phone
//byte rowPins[ROWS] = {5, 4, 3, 2}; //
//byte colPins[COLS] = {6, 7, 8}; //

//red phone
//byte rowPins[ROWS] = {8, 6, 5, 4}; //
//byte colPins[COLS] = {2, 3, 7}; //

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup() {
  Serial.begin(9600);

  pinMode(hangupPin, INPUT_PULLUP);
  receiverState = 1;

  AudioMemory(15);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.9);
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.micGain(17); //this affects the rms read, so calibrate this with yellThreshold

  mixer1.gain(0, 0.5); //mic volume in ear
  mixer1.gain(1, 0.5); //sine 1 volume
  mixer1.gain(2, 0.5); //sine 2 volume
  mixer1.gain(3, 0.5); //recording volume

  delay(1000);
}

void loop() {
  signalTone();
  checkRMS(); //listening for yells
  pickupHangup(); // looking for a pickup or hangup
  readKeypad(playerNumber); //checking for player keypresses
}


void readKeypad(int phoneID) {
  int dtmfCol[] = {440, 1209, 1336, 1477, 1209, 1336, 1477, 1209, 1336, 1477, 1209, 1336, 1477};
  int dtmfRow[] = {440, 697, 770, 852, 941, 697, 770, 852, 941, 697, 770, 852, 941};
  char player1[] = {'0', '1', '2', '3', 'q', 'w', 'e', 'a', 's', 'd', 'z', 'x', 'c'};
  char player2[] = {'0', '4', '5', '6', 'r', 't', 'y', 'f', 'g', 'h', 'v', 'b', 'n'};
  char player3[] = {'0', '7', '8', '9', 'u', 'i', 'o', 'j', 'k', 'l', 'm', ',', '.'};
  byte key = keypad.getKey();

  if (key) {
    sine1.frequency(dtmfCol[key]);
    sine2.frequency(dtmfRow[key]);
    dialToneOn = false;
  }


  if (keypad.getState()) {
    sine1.amplitude(toneVol);
    sine2.amplitude(toneVol);
  } else if (dialToneOn == false) {
    sine1.amplitude(0.0);
    sine2.amplitude(0.0);
  }




  switch (phoneID) {
    case 0:
      if (key) {
        Keyboard.write(player1[key]);
      }
      break;
    case 1:
      if (key) {
        Keyboard.write(player2[key]);
      }
      break;
    case 2:
      if (key) {
        Keyboard.write(player3[key]);
      }
      break;
  }


}

void pickupHangup() {
  if (digitalRead(hangupPin) == HIGH && receiverState == 1) {
    Keyboard.write(hangup[playerNumber]);
    Serial.println("hangup!");
    delay(30);
    receiverState = 0;
    dialToneOn = false;
    busySignalOn = false;
  } else if (digitalRead(hangupPin) == LOW && receiverState == 0) {
    Keyboard.write(pickup[playerNumber]);
    Serial.println("pickup!");
    delay(30);
    receiverState = 1;
    dialToneOn = true;
  } else {}

}

void checkRMS() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevMillis1 >= interval1) {
    prevMillis1 = currentMillis;
    if (rms1.available()) {
      float rms = rms1.read();

      if (rms > yellThreshold) {
        Keyboard.write(yell[playerNumber]);
        Serial.println("yell!");
        Serial.println(rms);

      }
    }
  }
}

void signalTone() {
  if (dialToneOn == true) {
    sine1.amplitude(0.7);
    sine2.amplitude(0.7);
    sine1.frequency(350);
    sine2.frequency(440);
  }
  if (busySignalOn == true) {
    sine1.frequency(480);
    sine2.frequency(620);
    sine1.amplitude(0.7);
    sine2.amplitude(0.7);
    delay(500);
    sine1.amplitude(0.0);
    sine2.amplitude(0.0);
    delay(500);
  }
}
