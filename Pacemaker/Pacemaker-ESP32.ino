#include <ESP32Servo.h>
#include <PulseSensorPlayground.h>

// ---- pins ----
const int PULSE_PIN      = 34;
const int ATRIUM_SERVO   = 25;
const int VENTRICLE_SERVO = 26;
const int ATRIUM_LED     = 27;
const int VENTRICLE_LED  = 33;

// ---- servo angles: small nudge only, not full rotation ----
const int SERVO_REST      = 90;   // resting position
const int SERVO_CONTRACT  = 105;  // nudge amount (~15 degrees, keep it gentle)

// ---- timing (all non-blocking, driven off millis()) ----
const unsigned long ATRIUM_VENTRICLE_DELAY = 120; // ms, atrium beats slightly before ventricle
const unsigned long BEAT_ANIM_DURATION     = 150; // ms, total length of one chamber's pulse animation
const int LED_MAX_BRIGHTNESS               = 255;
const unsigned long BPM_LOG_INTERVAL       = 1000; // ms, how often we log/refresh BPM (0.5-1s range)

// ---- pulse sensor setup ----
PulseSensorPlayground pulseSensor;
const int PULSE_THRESHOLD = 550; // adjust after testing with your sensor/finger

Servo atriumServo;
Servo ventricleServo;

// ---- chamber animation state ----
bool atriumActive = false;
unsigned long atriumStartTime = 0;

bool ventriclePending = false;
unsigned long ventriclePendingStart = 0;
bool ventricleActive = false;
unsigned long ventricleStartTime = 0;

// ---- periodic BPM logging ----
unsigned long lastBpmLogTime = 0;
int currentBpm = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(10); // match Uno's 0-1023 range so PULSE_THRESHOLD stays valid

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

  // Beat detection runs every loop pass, nothing blocks it anymore
  if (pulseSensor.sawStartOfBeat()) {
    currentBpm = pulseSensor.getBeatsPerMinute();

    atriumActive = true;
    atriumStartTime = now;

    ventriclePending = true;
    ventriclePendingStart = now;
  }

  // Fire the ventricle after its delay, without blocking anything else
  if (ventriclePending && (now - ventriclePendingStart >= ATRIUM_VENTRICLE_DELAY)) {
    ventriclePending = false;
    ventricleActive = true;
    ventricleStartTime = now;
  }

  updateChamber(atriumActive, atriumStartTime, ATRIUM_LED, atriumServo, now);
  updateChamber(ventricleActive, ventricleStartTime, VENTRICLE_LED, ventricleServo, now);

  // Log BPM on its own independent clock (every 0.5-1s), never tied to beat timing
  if (now - lastBpmLogTime >= BPM_LOG_INTERVAL) {
    lastBpmLogTime = now;
    Serial.print("BPM = ");
    Serial.println(currentBpm);
  }
}

// Drives one chamber's servo nudge + LED fade as a triangle pulse over BEAT_ANIM_DURATION,
// entirely from elapsed time -- no delay() anywhere.
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
