/*
  WIRING / PIN CONNECTIONS (Arduino Uno clone, powered via USB data cable):

  A0  -> Pulse Sensor SIGNAL (purple wire)
  D7  -> JQC-3F Relay IN            (most JQC-3F modules are ACTIVE-LOW, i.e. LOW = relay energized - flip RELAY_ON/RELAY_OFF below if yours is opposite)
  D8  -> L298N IN1   (Motor A / Balloon 1 direction)
  D9  -> L298N IN2   (Motor A / Balloon 1 direction)
  D10 -> L298N ENA   (Motor A / Balloon 1 speed, PWM)   <- remove L298N's ENA jumper cap
  D11 -> L298N ENB   (Motor B / Balloon 2 speed, PWM)   <- remove L298N's ENB jumper cap
  D12 -> L298N IN3   (Motor B / Balloon 2 direction)
  D13 -> L298N IN4   (Motor B / Balloon 2 direction)

  POWER:
  2x 18650 in SERIES (~7.4V) -> Relay COM
  Relay NO                   -> L298N 12V/VIN terminal   (relay cuts main battery power to the motors)
  Same 7.4V battery line      -> 5V Boost Converter IN
  Boost Converter 5V OUT      -> L298N 5V logic pin (remove L298N's onboard 5V regulator jumper first)
                               -> Pulse Sensor VCC
                               -> JQC-3F Relay VCC
  Battery NEG, Boost GND, L298N GND, Relay GND, Pulse Sensor GND, Arduino GND
                               -> all tied to one common ground rail

  L298N OUT1/OUT2 -> Motor 1 (air pump for Balloon/Chamber 1) -> silicone tube -> balloon 1
  L298N OUT3/OUT4 -> Motor 2 (air pump for Balloon/Chamber 2) -> silicone tube -> balloon 2

  NOTE: Pulse Sensor and Relay are powered from the boost 5V rail, not Arduino's own 5V pin,
  since USB power alone may not supply enough current for the sensor + relay coil + L298N logic.
*/

const int PULSE_PIN = A0;
const int RELAY_PIN = 7;
const int IN1 = 8;
const int IN2 = 9;
const int ENA = 10;
const int ENB = 11;
const int IN3 = 12;
const int IN4 = 13;

const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

int signalValue = 0;
unsigned long lastBeatTime = 0;
unsigned long IBI = 600;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 2;

int peakValue = 512;
int troughValue = 512;
int threshold = 550;
bool pulseHigh = false;
const unsigned long refractoryPeriod = 250;

unsigned long noPulseTimer = 0;
const unsigned long noPulseTimeout = 5000;
bool systemActive = false;

enum MotorPhase { IDLE, INFLATE, DEFLATE };
MotorPhase phase = IDLE;
unsigned long phaseStartTime = 0;
unsigned long inflateDuration = 200;
unsigned long deflateDuration = 300;
const int inflateSpeed = 220;
const int deflateSpeed = 180;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  stopMotors();
  Serial.begin(9600);
  lastBeatTime = millis();
  noPulseTimer = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= sampleInterval) {
    lastSampleTime = now;
    signalValue = analogRead(PULSE_PIN);

    if (signalValue > peakValue) peakValue = signalValue;
    if (signalValue < troughValue) troughValue = signalValue;

    threshold = troughValue + (peakValue - troughValue) * 0.5;

    if (!pulseHigh && signalValue > threshold && (now - lastBeatTime) > refractoryPeriod) {
      pulseHigh = true;
      IBI = now - lastBeatTime;
      lastBeatTime = now;
      noPulseTimer = now;
      systemActive = true;

      if (IBI > 300 && IBI < 2000) {
        inflateDuration = IBI * 0.3;
        deflateDuration = IBI * 0.4;
        Serial.print("BPM: ");
        Serial.println(60000 / IBI);
      }

      phase = INFLATE;
      phaseStartTime = now;

      peakValue = signalValue;
      troughValue = signalValue;
    }

    if (pulseHigh && signalValue < threshold) {
      pulseHigh = false;
    }

    if (peakValue > troughValue + 10) {
      peakValue -= 1;
      troughValue += 1;
    }
    peakValue = constrain(peakValue, 0, 1023);
    troughValue = constrain(troughValue, 0, 1023);
  }

  if (now - noPulseTimer > noPulseTimeout) {
    systemActive = false;
    phase = IDLE;
  }

  digitalWrite(RELAY_PIN, systemActive ? RELAY_ON : RELAY_OFF);

  switch (phase) {
    case INFLATE:
      setMotors(true, inflateSpeed);
      if (now - phaseStartTime >= inflateDuration) {
        phase = DEFLATE;
        phaseStartTime = now;
      }
      break;
    case DEFLATE:
      setMotors(false, deflateSpeed);
      if (now - phaseStartTime >= deflateDuration) {
        phase = IDLE;
        phaseStartTime = now;
      }
      break;
    case IDLE:
      stopMotors();
      break;
  }
}

void setMotors(bool forward, int speed) {
  digitalWrite(IN1, forward ? HIGH : LOW);
  digitalWrite(IN2, forward ? LOW : HIGH);
  digitalWrite(IN3, forward ? HIGH : LOW);
  digitalWrite(IN4, forward ? LOW : HIGH);
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
