#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// one heartbeat cycle: flat -> P wave -> sharp QRS spike -> T wave -> flat
const int8_t beatPattern[] = {
  0,0,0,1,2,3,2,1,0,-1,-2,-22,30,-15,2,4,
  6,8,9,8,6,4,2,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0
};
const int patternLen = sizeof(beatPattern) / sizeof(beatPattern[0]);
int patternIndex = 0;

int buf[SCREEN_WIDTH];
const int baseline = 40;

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  for (int i = 0; i < SCREEN_WIDTH; i++) buf[i] = baseline;
}

void loop() {
  // scroll buffer left, append next sample from the pattern
  for (int i = 0; i < SCREEN_WIDTH - 1; i++) buf[i] = buf[i + 1];
  buf[SCREEN_WIDTH - 1] = baseline + beatPattern[patternIndex];
  patternIndex = (patternIndex + 1) % patternLen;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("LIVE ECG SIM");

  for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
    display.drawLine(i, buf[i], i + 1, buf[i + 1], SSD1306_WHITE);
  }
  display.display();
  delay(15); // controls scroll speed / "heart rate" - lower = faster
}
