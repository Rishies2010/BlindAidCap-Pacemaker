/*
  Pulse-driven heart model — science expo project
  by IG @nqetm

  Hardware:
    Pulse sensor signal -> A0
    Atrium servo signal  -> D9
    Ventricle servo sig  -> D10
    Atrium LED (+resistor)    -> D3  (PWM)
    Ventricle LED (+resistor) -> D5  (PWM)
    18650 x2 pack -> Arduino VIN + GND
    Arduino 5V pin -> breadboard rail -> servo VCC + pulse sensor VCC
    All GNDs common (Arduino, servos, LEDs, battery pack)

*/

#include <Servo.h>
#include <PulseSensorPlayground.h>

const int PULSE_PIN      = A0;
const int ATRIUM_SERVO   = 9;
const int VENTRICLE_SERVO = 10;
const int ATRIUM_LED     = 3;
const int VENTRICLE_LED  = 5;

const int SERVO_REST      = 90;
const int SERVO_CONTRACT  = 105;
const unsigned long ATRIUM_VENTRICLE_DELAY = 120;
const unsigned long BEAT_ANIM_DURATION     = 150;
const int LED_MAX_BRIGHTNESS               = 255;
const unsigned long BPM_LOG_INTERVAL       = 1000;

PulseSensorPlayground pulseSensor;
const int PULSE_THRESHOLD = 550;

Servo atriumServo;
Servo ventricleServo;

bool atriumActive = false;
unsigned long atriumStartTime = 0;

bool ventriclePending = false;
unsigned long ventriclePendingStart = 0;
bool ventricleActive = false;
unsigned long ventricleStartTime = 0;

unsigned long lastBpmLogTime = 0;
int currentBpm = 0;

void setup() {
  Serial.begin(115200);

  atriumServo.attach(ATRIUM_SERVO);
  ventricleServo.attach(VENTRICLE_SERVO);
  atriumServo.write(SERVO_REST);
  ventricleServo.write(SERVO_REST);

  pinMode(ATRIUM_LED, OUTPUT);
  pinMode(VENTRICLE_LED, OUTPUT);

  pulseSensor.analogInput(PULSE_PIN);
  pulseSensor.setThreshold(PULSE_THRESHOLD);

  if (!pulseSensor.begin()) {
    Serial.println("Pulse sensor failed to initialize!");
  }
}

void loop() {
  unsigned long now = millis();

  if (pulseSensor.sawStartOfBeat()) {
    currentBpm = pulseSensor.getBeatsPerMinute();

    atriumActive = true;
    atriumStartTime = now;

    ventriclePending = true;
    ventriclePendingStart = now;
  }

  if (ventriclePending && (now - ventriclePendingStart >= ATRIUM_VENTRICLE_DELAY)) {
    ventriclePending = false;
    ventricleActive = true;
    ventricleStartTime = now;
  }

  updateChamber(atriumActive, atriumStartTime, ATRIUM_LED, atriumServo, now);
  updateChamber(ventricleActive, ventricleStartTime, VENTRICLE_LED, ventricleServo, now);

  if (now - lastBpmLogTime >= BPM_LOG_INTERVAL) {
    lastBpmLogTime = now;
    Serial.print("BPM = ");
    Serial.println(currentBpm);
  }
}

void updateChamber(bool &active, unsigned long &startTime, int ledPin, Servo &servo, unsigned long now) {
  if (!active) return;

  unsigned long elapsed = now - startTime;

  if (elapsed >= BEAT_ANIM_DURATION) {
    active = false;
    servo.write(SERVO_REST);
    analogWrite(ledPin, 0);
    return;
  }

  unsigned long half = BEAT_ANIM_DURATION / 2;
  int brightness;

  if (elapsed <= half) {
    brightness = map(elapsed, 0, half, 0, LED_MAX_BRIGHTNESS);
    servo.write(SERVO_CONTRACT);
  } else {
    brightness = map(elapsed, half, BEAT_ANIM_DURATION, LED_MAX_BRIGHTNESS, 0);
    servo.write(SERVO_REST);
  }

  analogWrite(ledPin, brightness);
}
