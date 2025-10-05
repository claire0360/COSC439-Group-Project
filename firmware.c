/*
 * File:     firwmware.c
 * Author:   Claire Kim
 *           Joan Teves
 * Company:  University of Canterbury Group 5
 * Date:     01/10/2025
 */

// ================================================================
// Ultrasonic A (single-pin SIG on D3) -> NeoPixel strip A on D13
// Ultrasonic B (TRIG=D6, ECHO=D7)    -> NeoPixel strip B on D2
// Buttons/LEDs (momentary, active-low):
//   D12 -> D11
//   D10 -> D9
//   D8  -> D5
//   D4  -> A0
//   A1  -> A2
//   A3  -> A4
//   A5  -> A6
// ================================================================
 
#include <Adafruit_NeoPixel.h>
#include <math.h>
 
// Piano notes
#define NOTE_C 262
#define NOTE_D 294
#define NOTE_E 330
#define NOTE_F 349
#define NOTE_G 392
#define NOTE_A 440
 
const int trigEchoPinA = 3;
const int trigPinB     = 6;
const int echoPinB     = 7;
 
const int LED_PIN_A   = 13;
const int LED_PIN_B   = 2;
const int LED_COUNT_A = 5;
const int LED_COUNT_B = 5;
 
Adafruit_NeoPixel stripA(LED_COUNT_A, LED_PIN_A, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripB(LED_COUNT_B, LED_PIN_B, NEO_GRB + NEO_KHZ800);
 
// --- Buttons & LEDs ---
const int buttonPin1 = 12;  const int ledPin1 = 11;
const int buttonPin2 = 10;  const int ledPin2 = 9;
const int buttonPin3 = 8;   const int ledPin3 = 5;
const int buttonPin4 = 4;   const int ledPin4 = A0;
const int buttonPin5 = A1;  const int ledPin5 = A2;
const int buttonPin6 = A3;  const int ledPin6 = A4;
const int SPEAKER_PIN = A5;
 
// Fix array order (button1 = C, button6 = A)
const int buttonPins[] = {buttonPin1, buttonPin2, buttonPin3, buttonPin4, buttonPin5, buttonPin6};
const int ledPins[] = {ledPin1, ledPin2, ledPin3, ledPin4, ledPin5, ledPin6};
const int notes[] = {NOTE_C, NOTE_D, NOTE_E, NOTE_F, NOTE_G, NOTE_A};
 
// Game variables
int turn = 0;
int inputSequence[100];
int currentStep = 0;
bool showingSequence = false;
bool waitingForInput = false;
unsigned long lastActionTime = 0;
const unsigned long STEP_DELAY = 800;
 
// Predefined melody (1=C, 2=D, 3=E, 4=F, 5=G, 6=A)
int melody[] = {3, 3, 4, 5, 5, 4, 3, 2, 1, 1, 2, 3, 3, 2, 2};
int melodyLength = sizeof(melody) / sizeof(melody[0]);
 
// Timing / thresholds
const unsigned long ECHO_TIMEOUT_US = 30000UL;
const float SOUND_CM_PER_US = 0.0343f;
const float THRESH_CM = 3.0f;
 
// Breathing effect variables for Sensor A
static float pulseSpeed = 0.5;  // Larger value gives faster pulse.
uint8_t hueA = 15;  // Start hue at valueMin.
uint8_t satA = 230;  // Start saturation at valueMin.
float valueMin = 120.0;  // Pulse minimum value
uint8_t hueB = 95;  // End hue at valueMax.
uint8_t satB = 255;  // End saturation at valueMax.
float valueMax = 255.0;  // Pulse maximum value
uint8_t hue = hueA;
uint8_t sat = satA;
float val = valueMin;
uint8_t hueDelta = hueA - hueB;
static float delta = (valueMax - valueMin) / 2.35040238;
unsigned long lastBreathUpdate = 0;
const unsigned long BREATH_INTERVAL = 30; // ms between updates
 
void setup() {
  Serial.begin(9600);
 
  // --- Ultrasonic setup ---
  pinMode(trigEchoPinA, OUTPUT); digitalWrite(trigEchoPinA, LOW);
  pinMode(trigPinB, OUTPUT); digitalWrite(trigPinB, LOW);
  pinMode(echoPinB, INPUT);
 
  // --- LED strip setup ---
  stripA.begin(); stripB.begin();
  stripA.setBrightness(40); stripB.setBrightness(40);
  stripA.show(); stripB.show();
 
  // --- Buttons & LEDs ---
  for (int i = 0; i < 6; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(ledPins[i], OUTPUT);
  }
 
  pinMode(SPEAKER_PIN, OUTPUT);
  playStartupMelody();
}
 
void loop() {
  // Always run ultrasonic sensors (non-blocking)
  runUltrasonicSensing();
  
  // Always handle button presses (non-blocking)
  handleButtons();
  
  // Run game logic (non-blocking)
  runMemoryGame();
  
  delay(50);
}
 
void runUltrasonicSensing() {
  // Ultrasonic sensor A
  float dA = readDistanceSinglePin(trigEchoPinA, ECHO_TIMEOUT_US);
  
  if (!isnan(dA) && dA > THRESH_CM) {
    // Sensor A activated - show breathing effect
    showBreathingEffect(stripA, LED_COUNT_A);
  } else {
    // Sensor A not active - clear strip
    clearStrip(stripA, LED_COUNT_A);
  }
 
  // Ultrasonic sensor B (normal rainbow)
  float dB = readDistanceTrigEcho(trigPinB, echoPinB, ECHO_TIMEOUT_US);
  if (!isnan(dB) && dB > THRESH_CM) {
    showRainbowStatic(stripB, LED_COUNT_B);
  } else {
    clearStrip(stripB, LED_COUNT_B);
  }
 
  // Display distances occasionally
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.print("A: "); printDistance(dA);
    Serial.print(" | B: "); printDistance(dB);
    Serial.println();
    lastPrint = millis();
  }
}
 
void showBreathingEffect(Adafruit_NeoPixel &strip, int numLeds) {
  if (millis() - lastBreathUpdate > BREATH_INTERVAL) {
    // Calculate breathing pulse
    float dV = ((exp(sin(pulseSpeed * millis()/2000.0*PI)) -0.36787944) * delta);
    val = valueMin + dV;
    hue = map(val, valueMin, valueMax, hueA, hueB);
    sat = map(val, valueMin, valueMax, satA, satB);
    
    // Convert HSV to RGB
    uint32_t color = colorWheelBreathing(hue, sat, val);
    
    // Apply to all LEDs in the strip
    for (int i = 0; i < numLeds; i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
    
    lastBreathUpdate = millis();
  }
}
 
uint32_t colorWheelBreathing(byte hue, byte sat, byte val) {
  // Convert HSV to RGB (simplified version)
  hue = 255 - hue;
  if (hue < 85) {
    return Adafruit_NeoPixel::Color(
      map(val, 0, 255, 0, (255 - hue * 3)),
      map(val, 0, 255, 0, 0),
      map(val, 0, 255, 0, (hue * 3))
    );
  } else if (hue < 170) {
    hue -= 85;
    return Adafruit_NeoPixel::Color(
      map(val, 0, 255, 0, 0),
      map(val, 0, 255, 0, (hue * 3)),
      map(val, 0, 255, 0, (255 - hue * 3))
    );
  } else {
    hue -= 170;
    return Adafruit_NeoPixel::Color(
      map(val, 0, 255, 0, (hue * 3)),
      map(val, 0, 255, 0, (255 - hue * 3)),
      map(val, 0, 255, 0, 0)
    );
  }
}
 
void handleButtons() {
  for (int i = 0; i < 6; i++) {
    bool pressed = digitalRead(buttonPins[i]) == LOW;
    digitalWrite(ledPins[i], pressed);
    if (pressed) {
      tone(SPEAKER_PIN, notes[i], 200);
      if (waitingForInput) {
        checkGameInput(i + 1);
      }
    }
  }
}
 
void runMemoryGame() {
  if (turn >= melodyLength) {
    winSequence();
    turn = 0;
    currentStep = 0;
    showingSequence = false;
    waitingForInput = false;
    return;
  }
  
  if (!showingSequence && !waitingForInput) {
    showingSequence = true;
    currentStep = 0;
    Serial.print("Turn "); Serial.println(turn + 1);
  }
  
  if (showingSequence && millis() - lastActionTime > STEP_DELAY) {
    if (currentStep <= turn) {
      playNote(melody[currentStep]);
      currentStep++;
      lastActionTime = millis();
    } else {
      showingSequence = false;
      waitingForInput = true;
      currentStep = 0;
      Serial.println("Your turn!");
    }
  }
}
 
void checkGameInput(int buttonPressed) {
  if (!waitingForInput) return;
  
  int expectedNote = melody[currentStep];
  
  if (buttonPressed == expectedNote) {
    Serial.println("Correct!");
    currentStep++;
    
    if (currentStep > turn) {
      turn++;
      waitingForInput = false;
      Serial.println("Good! Next level...");
      delay(1000);
    }
  } else {
    Serial.println("Wrong! Try again.");
    failSequence();
  }
}
 
void playNote(int noteIndex) {
  int arrayIndex = noteIndex - 1;
  if (arrayIndex >= 0 && arrayIndex < 6) {
    digitalWrite(ledPins[arrayIndex], HIGH);
    tone(SPEAKER_PIN, notes[arrayIndex], 300);
    delay(300);
    digitalWrite(ledPins[arrayIndex], LOW);
    noTone(SPEAKER_PIN);
  }
}
 
void failSequence() {
  Serial.println("FAIL! Restarting...");
  
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 6; j++) digitalWrite(ledPins[j], HIGH);
    tone(SPEAKER_PIN, NOTE_C, 300);
    delay(400);
    for (int j = 0; j < 6; j++) digitalWrite(ledPins[j], LOW);
    tone(SPEAKER_PIN, NOTE_G, 300);
    delay(400);
  }
  noTone(SPEAKER_PIN);
  
  turn = 0;
  currentStep = 0;
  showingSequence = false;
  waitingForInput = false;
  delay(1000);
}
 
void winSequence() {
  Serial.println("YOU WIN!");
  
  for (int i = 0; i < 6; i++) digitalWrite(ledPins[i], HIGH);
  
  tone(SPEAKER_PIN, NOTE_E, 400);
  delay(500);
  tone(SPEAKER_PIN, NOTE_G, 400);
  delay(500);
  tone(SPEAKER_PIN, NOTE_E, 400);
  delay(500);
  tone(SPEAKER_PIN, NOTE_C, 600);
  delay(700);
  noTone(SPEAKER_PIN);
  
  for (int i = 0; i < 6; i++) digitalWrite(ledPins[i], LOW);
  delay(2000);
}
 
void playStartupMelody() {
  for (int i = 0; i < 15; i++) {
    int noteIndex = melody[i] - 1;
    if (noteIndex >= 0 && noteIndex < 6) {
      digitalWrite(ledPins[noteIndex], HIGH);
      tone(SPEAKER_PIN, notes[noteIndex], 300);
      delay(350);
      digitalWrite(ledPins[noteIndex], LOW);
      delay(50);
    }
  }
  delay(500);
}
 
// ================= Ultrasonic Helpers =================
float readDistanceSinglePin(int sigPin, unsigned long timeoutUs) {
  pinMode(sigPin, OUTPUT);
  digitalWrite(sigPin, LOW); delayMicroseconds(2);
  digitalWrite(sigPin, HIGH); delayMicroseconds(10);
  digitalWrite(sigPin, LOW);
  pinMode(sigPin, INPUT);
  unsigned long duration = pulseIn(sigPin, HIGH, timeoutUs);
  if (duration == 0) return NAN;
  float cm = (duration * SOUND_CM_PER_US) / 2.0f;
  if (cm < 0.5f) cm = 0.0f;
  return cm;
}
 
float readDistanceTrigEcho(int trigPin, int echoPin, unsigned long timeoutUs) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  unsigned long duration = pulseIn(echoPin, HIGH, timeoutUs);
  if (duration == 0) return NAN;
  float cm = (duration * SOUND_CM_PER_US) / 2.0f;
  if (cm < 0.5f) cm = 0.0f;
  return cm;
}
 
void printDistance(float cm) {
  if (isnan(cm)) Serial.print("—");
  else { Serial.print(cm, 2); Serial.print(" cm"); }
}
 
void clearStrip(Adafruit_NeoPixel &s, int n) {
  for (int i = 0; i < n; i++) s.setPixelColor(i, 0);
  s.show();
}
 
void showRainbowStatic(Adafruit_NeoPixel &s, int n) {
  for (int i = 0; i < n; i++) {
    uint8_t pos = (uint8_t)(i * (255 / max(1, n - 1)));
    s.setPixelColor(i, colorWheel(s, pos));
  }
  s.show();
}
 
uint32_t colorWheel(Adafruit_NeoPixel &s, byte pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return s.Color(255 - pos * 3, 0, pos * 3);
  } else if (pos < 170) {
    pos -= 85;
    return s.Color(0, pos * 3, 255 - pos * 3);
  } else {
    pos -= 170;
    return s.Color(pos * 3, 255 - pos * 3, 0);
  }
}