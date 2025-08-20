#include <Servo.h>
#include <stdio.h>
#include <math.h>
#include <Wire.h>
#include <Volume.h>

//Notes and Beats
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523

#define BEAT_1 150
#define BEAT_2 300  //BEAT_1* 2;
#define BEAT_3 450  //BEAT_1*3
#define BEAT_6 900  //BEAT_1*6

//pins and motors
#define startButtonPin 2
#define startBtnLEDPin 3
#define stopButtonPin 4
#define stopBtnLEDPin 8
#define syncButtonPin 6
#define syncBtnLEDPin 7
#define speakerPin1 5 //8
#define leftwingPIN 10
#define rightwingPIN 11

const int volumePin = A0;
const int tempoPin = A1;
const int octavePin = A2;

// "Row Row Row Your Boat" melody
// Notes for the song (using a simplified melody)
int melody[] = { NOTE_C4, NOTE_C4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_C5, NOTE_C5, NOTE_C5, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_C4, NOTE_C4, NOTE_C4, NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_C4 };

// Note durations (in milliseconds)
int beat[] = { BEAT_3, BEAT_3, BEAT_2, BEAT_1, BEAT_3, BEAT_2, BEAT_1, BEAT_2, BEAT_1, BEAT_6, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_1, BEAT_2, BEAT_1, BEAT_2, BEAT_1, BEAT_6 };

//Varibles
int lastStartState = LOW;
int startBtnState;
int stopBtnState;
int syncBtnState;

Volume vol;
int volumeValue = 0;
int outputVolumeValue = 0;
unsigned long lastVolumeUpdate = 0;

int tempoValue = 0;
int outputTempoValue = 0;
unsigned long lastToneUpdate = 0;

int octaveValue = 0;
int outputOctaveValue = 0;
unsigned long lastOctaveUpdate = 0;

int currentTone = 0;
int isMute = 0;
byte prevVol = 0;
int pos;
float mv = 0;


// Global variables for timing
unsigned long nextFlapTime = 0;
unsigned long nextNoteTime = 0;
int currentWingPos = 0;
int currentFlapDir = 1;  // 1 = up, -1 = down
int currentNote = 0;

bool notePlaying = false;
bool fly = true;
bool playing = false;

//setup the servo output
Servo left_wing;
Servo right_wing;

//states
enum state { ST_IDLE = 0,
             ST_STOP = 1,
             ST_PLAY = 2,
             ST_SYNC = 3,
             ST_OCTAVE_SYNC = 4
};
////// setting the begining state
int pastState = ST_PLAY;
int currentState = ST_PLAY;

void setup() {
  Serial.print("setup");
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(startBtnLEDPin, OUTPUT);
  pinMode(stopButtonPin, INPUT_PULLUP);
  pinMode(stopBtnLEDPin, OUTPUT);
  // pinMode(syncButtonPin, INPUT_PULLUP);
  // pinMode(syncBtnLEDPin, OUTPUT);

  pinMode(speakerPin1, OUTPUT);
  pinMode(volumePin, INPUT);
  pinMode(tempoPin, INPUT);
  pinMode(octavePin, INPUT);

  left_wing.attach(leftwingPIN);
  right_wing.attach(rightwingPIN);

  pinMode(LED_BUILTIN, OUTPUT);

  vol.begin();

  Serial.begin(9600);
}

void loop() {
  //Serial.print("looping");
  startBtnState = digitalRead(startButtonPin);
  stopBtnState  = digitalRead(stopButtonPin);
  // syncBtnState  = digitalRead(stopButtonPin);

  // Potentiometers 
  volumeValue = analogRead(volumePin);
  outputVolumeValue = map(volumeValue, 0, 1023, 0, 255);

  tempoValue = analogRead(tempoPin);
  outputTempoValue = map(tempoValue, 0, 1023, 0, 255);

  octaveValue = analogRead(octavePin);
  outputOctaveValue = map(octaveValue, 0, 1023, 0, 255);

  mv = constrain(analogRead(speakerPin1) / 10, 0, 100) / 100.00; 
 

  // Start button pressed
  if (startBtnState == LOW && !playing) {
    playing = true;
    currentNote = 0;
    notePlaying = false;
    currentWingPos = 0;
    currentFlapDir = 1;
    nextNoteTime = millis();
    nextFlapTime = millis();
    
    digitalWrite(startBtnLEDPin, HIGH);
    digitalWrite(stopBtnLEDPin, LOW);
    Serial.print("Start button pressed \n");
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Stop button pressed
  if (stopBtnState == LOW && playing) {
    playing = false;
    // stop sound immediately
    vol.noTone(); 
    //noTone(speakerPin1);  
    
    digitalWrite(startBtnLEDPin, LOW);
    digitalWrite(stopBtnLEDPin, HIGH);
    Serial.print("Stop button pressed \n");
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (playing) {
    //rial.print("playing \n");
    updateNote();       // Handles melody timing
    updateFlapping();   // Handles servo timing
  }
}

void updateNote() {
  unsigned long now = millis();
  if (currentNote >= 27) {
    // Song is over, stop and reset
    playing = false;
    vol.noTone(); 
    // noTone(speakerPin1);
    digitalWrite(startBtnLEDPin, LOW);
    digitalWrite(stopBtnLEDPin, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    return;
  }
  if (!notePlaying && now >= nextNoteTime) {
    // Start next note
    // tone(speakerPin1, melody[currentNote]);
    vol.tone(melody[currentNote], 255 * mv);//vol.tone(pin, frequency, volume);

    notePlaying = true;
    nextNoteTime += beat[currentNote];
  } else if (notePlaying && now >= nextNoteTime) {
    // End current note, prepare for next
    // noTone(speakerPin1);
    vol.noTone(); 
    notePlaying = false;
    currentNote++;
    nextNoteTime += 20;  // Very brief delay between notes
  }
}

void updateFlapping() {
  unsigned long now = millis();
  if (now >= nextFlapTime) {
    // Move the servos
    currentWingPos += currentFlapDir;
    left_wing.write(currentWingPos);
    right_wing.write(currentWingPos);
    // Reverse direction at limits
    if (currentWingPos >= 45 || currentWingPos <= 0) {
      currentFlapDir *= -1;
    }
    // Schedule next update
    nextFlapTime = now + 15;  // Controls flap speed
  }
}

