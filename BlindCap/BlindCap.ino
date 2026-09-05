const int trigPin = 9;
const int echoPin = 10;
const int buzzerPin = 6; // moved to PWM-capable pin
const int ledPin = LED_BUILTIN;

long duration;
int distance = 999;

unsigned long lastMeasureTime = 0;
const unsigned long measureInterval = 60;

unsigned long lastBeepToggle = 0;
bool buzzerState = false;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  analogWrite(buzzerPin, 255); // fully off (active-LOW: 255=HIGH=off)
  digitalWrite(ledPin, LOW);
  Serial.begin(9600);
}

void loop() {
  unsigned long now = millis();
  if (now - lastMeasureTime >= measureInterval) {
    lastMeasureTime = now;

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH, 30000);

    if (duration == 0) {
      Serial.println("Too Far! (Or Close)");
      distance = 999;
    } else {
      distance = duration * 0.034 / 2;
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
  }

  if (distance > 0 && distance < 10) {
    analogWrite(buzzerPin, 0);   // max volume (fully LOW/on)
    digitalWrite(ledPin, HIGH);
  } 
  else if (distance < 50) {
    updateBeep(now, 80, 80, 50);   // medium-loud
  } 
  else if (distance < 100) {
    updateBeep(now, 100, 400, 110);
  } 
  else if (distance < 150) {
    updateBeep(now, 100, 400, 145);
  } 
  else if (distance < 200) {
    updateBeep(now, 100, 400, 180);
  }
  else {
    analogWrite(buzzerPin, 255); // off
    digitalWrite(ledPin, LOW);
  }
}

void updateBeep(unsigned long now, int onTime, int offTime, int volume) {
  unsigned long interval = buzzerState ? onTime : offTime;
  if (now - lastBeepToggle >= interval) {
    buzzerState = !buzzerState;
    if (buzzerState) {
      analogWrite(buzzerPin, volume); // lower number = louder (active-LOW)
      digitalWrite(ledPin, HIGH);
    } else {
      analogWrite(buzzerPin, 255); // off
      digitalWrite(ledPin, LOW);
    }
    lastBeepToggle = now;
  }
}