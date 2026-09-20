// ============================================================
// Pacemaker Demo
// ESP32 Firmware + LittleFS Web Dashboard
//
// Web:
//   http://192.168.4.1 or http://pacemaker.local
//
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "driver/dac.h"
#include <esp_timer.h>
#include <math.h>
#define PULSE_PIN 34

#define SERVO_ATRIUM_PIN 18
#define SERVO_VENTRICLE_PIN 19

#define LED_ATRIUM_PIN 27
#define LED_VENTRICLE_PIN 33

#define LED_EXTRA1_PIN 4
#define LED_EXTRA2_PIN 32

#define BUZZER_PIN 14

#define AMP_OUT_PIN 25

#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
    );
const char* AP_SSID = "Pacemaker";
const char* AP_PASSWORD = "87654321";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);
Preferences preferences;
struct Settings {

	int volume;

	int targetBPM;

	int normalLow;
	int normalHigh;

	int atriumRest;
	int atriumBeat;

	int ventricleRest;
	int ventricleBeat;

	String theme;
	String accent;

};

Settings settings;
Servo atriumServo;
Servo ventricleServo;

// Servo rest/beat angles now live in Settings (user-configurable via web UI).
// Defaults applied in loadSettings(): atriumRest/atriumBeat/ventricleRest/ventricleBeat.

const unsigned long PR_DELAY_MS = 160;
const unsigned long BEAT_DURATION_MS = 120;

const unsigned long CORRECTION_STEP_MS = 300;

const unsigned long DISPLAY_REFRESH_MS = 200;
const int SAMPLE_RATE_HZ = 100;
const int SAMPLE_INTERVAL_MS = 1000 / SAMPLE_RATE_HZ;

const int WINDOW_SECONDS = 4;
const int WINDOW_SIZE = SAMPLE_RATE_HZ * WINDOW_SECONDS;

const int MIN_PEAK_DISTANCE_SAMPLES = 30;

const unsigned long RECOMPUTE_INTERVAL_MS = 1000;

const int MIN_VALID_PEAKS = 3;

const int MIN_PLAUSIBLE_BPM = 30;
const int MAX_PLAUSIBLE_BPM = 200;

const unsigned long REPOSITION_HOLD_MS = 1500;int sampleBuffer[WINDOW_SIZE];

int bufferIndex = 0;
int samplesCollected = 0;

float smoothedSample = 0;

unsigned long lastComputeTime = 0;

int rawBPM = 75;

int lastRawSensorValue = 0;

float signalQuality = 0;

bool pacing = false;

float effectiveBPM = 75;

unsigned long lastBadReadingTime = 0;

// --- Pulse simulation / manual override ---
// The raw analog pulse sensor is noisy/unreliable, but the signal-quality
// estimate (derived from ADC amplitude range) is accurate. So above the
// "good contact" threshold we simulate a plausible BPM instead of trusting
// the broken peak detector; below it, we report reposition/low quality.
const float SIGNAL_QUALITY_GOOD_THRESHOLD = 95.0;

enum FakePulseState { FAKE_NORMAL, FAKE_EXCURSION };

FakePulseState fakePulseState = FAKE_NORMAL;
float fakeCurrentBPM = 75;
float fakeTargetBPM = 75;
unsigned long fakeStateUntil = 0;
unsigned long lastFakeUpdate = 0;
const unsigned long FAKE_UPDATE_INTERVAL_MS = 400;

volatile bool manualBPMEnabled = false;
volatile int manualBPMValue = 75;

unsigned long bootTime = 0;
portMUX_TYPE bpmMutex =
    portMUX_INITIALIZER_UNLOCKED;

portMUX_TYPE audioMutex =
    portMUX_INITIALIZER_UNLOCKED;
unsigned long lastAtriumBeat = 0;

unsigned long lastCorrectionStep = 0;

unsigned long lastDisplayUpdate = 0;

bool atriumContracting = false;

bool ventricleContracting = false;

bool ventriclePending = false;unsigned long atriumBeatStart = 0;

unsigned long ventricleBeatStart = 0;

unsigned long ventricleTriggerTime = 0;
bool buzzerOn = false;

unsigned long buzzerOffTime = 0;
const int AUDIO_SAMPLE_RATE = 8000;

const float LUB_FREQ = 70.0;
const float LUB_DECAY = 18.0;

const unsigned long LUB_DURATION_MS = 140;const float DUB_FREQ = 110.0;
const float DUB_DECAY = 25.0;

const unsigned long DUB_DURATION_MS = 100;const int DAC_IDLE_LEVEL = 128;volatile float currentVolume = 0.6;

volatile bool notePlaying = false;

volatile float noteFreq = 0;

volatile float noteDecay = 0;

volatile unsigned long noteStartSample = 0;

volatile unsigned long noteDurationSamples = 0;

volatile unsigned long audioSampleCounter = 0;
const int MAX_LOGS = 20;

struct EventLog {

	unsigned long time;
	String event;

};

EventLog eventLogs[MAX_LOGS];

int logCount = 0;
void loadSettings();

void saveSettings();

void addEvent(String event);

String computeStatusText(
    int currentBPM,
    bool reposition
);

void setupWiFi();

void setupWebServer();

void webServerTask(void* parameter);

void handleData();

void handleOLED();

void handleSystem();

void handleSettings();

void handleSaveSettings();

void handleManualBPM();

void handleSetManualBPM();

void handleLogs();

void handleRestart();

void serveFile(
    String path,
    String contentType
);

int getRawBPM();

bool isRepositionActive();

void addSample(int value);

void computeBPMFromWindow();

void updatePulseSimulation(unsigned long now);

void pulseSensorTask(void* parameter);

void playNote(
    float freq,
    float decay,
    unsigned long durationMs
);

void playLub();

void playDub();

void audioTask(void* parameter);

void updatePacingLogic(unsigned long now);

void runBeatScheduler(unsigned long now);

void triggerBuzzer(
    int freq,
    unsigned long durationMs
);

void updateBuzzerTimer(unsigned long now);

void updateDisplay();
void loadSettings() {

	preferences.begin(
	    "pacemaker",
	    false
	);	settings.volume =
	    preferences.getInt(
	        "volume",
	        60
	    );	settings.targetBPM =
	    preferences.getInt(
	        "target",
	        75
	    );	settings.normalLow =
	    preferences.getInt(
	        "normalLow",
	        60
	    );	settings.normalHigh =
	    preferences.getInt(
	        "normalHigh",
	        100
	    );	settings.atriumRest =
	    preferences.getInt(
	        "atriumRest",
	        90
	    );	settings.atriumBeat =
	    preferences.getInt(
	        "atriumBeat",
	        70
	    );	settings.ventricleRest =
	    preferences.getInt(
	        "ventRest",
	        90
	    );	settings.ventricleBeat =
	    preferences.getInt(
	        "ventBeat",
	        70
	    );	settings.theme =
	    preferences.getString(
	        "theme",
	        "system"
	    );	settings.accent =
	    preferences.getString(
	        "accent",
	        "violet"
	    );	preferences.end();	currentVolume =
	    settings.volume / 100.0;	effectiveBPM =
	    settings.targetBPM;

}void saveSettings() {

	preferences.begin(
	    "pacemaker",
	    false
	);	preferences.putInt(
	    "volume",
	    settings.volume
	);	preferences.putInt(
	    "target",
	    settings.targetBPM
	);	preferences.putInt(
	    "normalLow",
	    settings.normalLow
	);	preferences.putInt(
	    "normalHigh",
	    settings.normalHigh
	);	preferences.putInt(
	    "atriumRest",
	    settings.atriumRest
	);	preferences.putInt(
	    "atriumBeat",
	    settings.atriumBeat
	);	preferences.putInt(
	    "ventRest",
	    settings.ventricleRest
	);	preferences.putInt(
	    "ventBeat",
	    settings.ventricleBeat
	);	preferences.putString(
	    "theme",
	    settings.theme
	);	preferences.putString(
	    "accent",
	    settings.accent
	);	preferences.end();

}
void addEvent(String event) {

	unsigned long now =
	    millis();	if (logCount < MAX_LOGS) {

		eventLogs[logCount].time =
		    now;

		eventLogs[logCount].event =
		    event;

		logCount++;

	}

	else {

		for (
		    int i = 1;
		    i < MAX_LOGS;
		    i++
		) {

			eventLogs[i - 1] =
			    eventLogs[i];

		}		eventLogs[MAX_LOGS - 1].time =
		    now;

		eventLogs[MAX_LOGS - 1].event =
		    event;

	}

}
String computeStatusText(
    int currentBPM,
    bool reposition
) {

	if (reposition) {

		return "REPOSITION";

	}	if (
	    currentBPM <
	    settings.normalLow
	) {

		return "LOW";

	}	if (
	    currentBPM >
	    settings.normalHigh
	) {

		return "HIGH";

	}	return "NORMAL";

}
void setupWiFi() {

	WiFi.mode(WIFI_AP);	WiFi.softAPConfig(
	    AP_IP,
	    AP_GATEWAY,
	    AP_SUBNET
	);	WiFi.softAP(
	    AP_SSID,
	    AP_PASSWORD
	);	addEvent(
	    "WiFi access point started"
	);

	if (!MDNS.begin("pacemaker")) {
		Serial.println("Error starting mDNS");
	} else {
		Serial.println("mDNS started!");
		Serial.println("Open: http://pacemaker.local");
	}

}
void serveFile(
    String path,
    String contentType
) {

	if (
	    LittleFS.exists(path)
	) {

		File file =
		    LittleFS.open(
		        path,
		        "r"
		    );		server.streamFile(
		    file,
		    contentType
		);		file.close();

		return;

	}	server.send(
	    404,
	    "text/plain",
	    "File not found"
	);

}
void handleData() {

	StaticJsonDocument<1024>
	doc;	int bpm =
	    getRawBPM();	bool reposition =
	    isRepositionActive();	String status =
	    computeStatusText(
	        bpm,
	        reposition
	    );	doc["bpm"] =
	    bpm;	doc["effectiveBPM"] =
	    (int)effectiveBPM;	doc["status"] =
	    status;	doc["pacing"] =
	    pacing;	doc["targetBPM"] =
	    settings.targetBPM;	doc["normalLow"] =
	    settings.normalLow;	doc["normalHigh"] =
	    settings.normalHigh;	doc["signalQuality"] =
	    signalQuality;	doc["rawSensor"] =
	    lastRawSensorValue;	doc["reposition"] =
	    reposition;	doc["atriumActive"] =
	    atriumContracting;	doc["ventricleActive"] =
	    ventricleContracting;	doc["volume"] =
	    settings.volume;	doc["manualBPM"] =
	    manualBPMEnabled;	doc["manualBPMValue"] =
	    manualBPMValue;	doc["uptime"] =
	    millis();	String output;

	serializeJson(
	    doc,
	    output
	);	server.send(
	    200,
	    "application/json",
	    output
	);

}
void handleSystem() {

	StaticJsonDocument<1536>
	doc;	doc["chipModel"] =
	    ESP.getChipModel();	doc["chipRevision"] =
	    ESP.getChipRevision();	doc["cores"] =
	    ESP.getChipCores();	doc["cpuMHz"] =
	    ESP.getCpuFreqMHz();	doc["freeHeap"] =
	    ESP.getFreeHeap();	doc["minFreeHeap"] =
	    ESP.getMinFreeHeap();	doc["maxAllocHeap"] =
	    ESP.getMaxAllocHeap();	doc["flashSize"] =
	    ESP.getFlashChipSize();	doc["flashSpeed"] =
	    ESP.getFlashChipSpeed();	doc["sdkVersion"] =
	    ESP.getSdkVersion();	doc["uptime"] =
	    millis();	doc["wifiMode"] =
	    "Access Point";	doc["ssid"] =
	    AP_SSID;	doc["ip"] =
	    WiFi.softAPIP().toString();	doc["connectedClients"] =
	    WiFi.softAPgetStationNum();	doc["mac"] =
	    WiFi.softAPmacAddress();	doc["filesystemTotal"] =
	    LittleFS.totalBytes();	doc["filesystemUsed"] =
	    LittleFS.usedBytes();	String output;

	serializeJson(
	    doc,
	    output
	);	server.send(
	    200,
	    "application/json",
	    output
	);

}
void handleOLED() {

  const uint8_t* buffer =
    display.getBuffer();

  const size_t len =
    SCREEN_WIDTH *
    (SCREEN_HEIGHT / 8);

  server.sendHeader(
    "Cache-Control",
    "no-store, no-cache, must-revalidate"
  );

  server.send_P(
    200,
    "application/octet-stream",
    (PGM_P)buffer,
    len
  );

}
void handleSettings() {

	StaticJsonDocument<512>
	doc;	doc["volume"] =
	    settings.volume;	doc["targetBPM"] =
	    settings.targetBPM;	doc["normalLow"] =
	    settings.normalLow;	doc["normalHigh"] =
	    settings.normalHigh;	doc["atriumRest"] =
	    settings.atriumRest;	doc["atriumBeat"] =
	    settings.atriumBeat;	doc["ventricleRest"] =
	    settings.ventricleRest;	doc["ventricleBeat"] =
	    settings.ventricleBeat;	doc["theme"] =
	    settings.theme;	doc["accent"] =
	    settings.accent;	String output;

	serializeJson(
	    doc,
	    output
	);	server.send(
	    200,
	    "application/json",
	    output
	);

}
void handleSaveSettings() {

	if (
	    !server.hasArg("plain")
	) {

		server.send(
		    400,
		    "application/json",
		    "{\"error\":\"Missing JSON body\"}"
		);

		return;

	}	StaticJsonDocument<1024>
	doc;	DeserializationError error =
	    deserializeJson(
	        doc,
	        server.arg("plain")
	    );	if (error) {

		server.send(
		    400,
		    "application/json",
		    "{\"error\":\"Invalid JSON\"}"
		);

		return;

	}	if (
	    doc.containsKey("volume")
	) {

		settings.volume =
		    constrain(
		        doc["volume"],
		        0,
		        100
		    );		portENTER_CRITICAL(
		    &audioMutex
		);		currentVolume =
		    settings.volume / 100.0;		portEXIT_CRITICAL(
		    &audioMutex
		);

	}	if (
	    doc.containsKey("targetBPM")
	) {

		settings.targetBPM =
		    constrain(
		        doc["targetBPM"],
		        30,
		        200
		    );

	}	if (
	    doc.containsKey("normalLow")
	) {

		settings.normalLow =
		    constrain(
		        doc["normalLow"],
		        30,
		        180
		    );

	}	if (
	    doc.containsKey("normalHigh")
	) {

		settings.normalHigh =
		    constrain(
		        doc["normalHigh"],
		        40,
		        220
		    );

	}	if (
	    settings.normalLow >=
	    settings.normalHigh
	) {

		settings.normalHigh =
		    settings.normalLow + 1;

	}	if (
	    doc.containsKey("atriumRest")
	) {

		settings.atriumRest =
		    constrain(
		        doc["atriumRest"],
		        0,
		        180
		    );

	}	if (
	    doc.containsKey("atriumBeat")
	) {

		settings.atriumBeat =
		    constrain(
		        doc["atriumBeat"],
		        0,
		        180
		    );

	}	if (
	    doc.containsKey("ventricleRest")
	) {

		settings.ventricleRest =
		    constrain(
		        doc["ventricleRest"],
		        0,
		        180
		    );

	}	if (
	    doc.containsKey("ventricleBeat")
	) {

		settings.ventricleBeat =
		    constrain(
		        doc["ventricleBeat"],
		        0,
		        180
		    );

	}	if (
	    !atriumContracting
	) {

		atriumServo.write(
		    settings.atriumRest
		);

	}	if (
	    !ventricleContracting
	) {

		ventricleServo.write(
		    settings.ventricleRest
		);

	}	if (
	    doc.containsKey("theme")
	) {

		settings.theme =
		    doc["theme"]
		    .as<String>();

	}	if (
	    doc.containsKey("accent")
	) {

		settings.accent =
		    doc["accent"]
		    .as<String>();

	}	saveSettings();	addEvent(
	    "Settings updated"
	);	server.send(
	    200,
	    "application/json",
	    "{\"success\":true}"
	);

}
void handleManualBPM() {

	StaticJsonDocument<256>
	doc;	doc["enabled"] =
	    manualBPMEnabled;	doc["bpm"] =
	    manualBPMValue;	String output;

	serializeJson(
	    doc,
	    output
	);	server.send(
	    200,
	    "application/json",
	    output
	);

}
void handleSetManualBPM() {

	if (
	    !server.hasArg("plain")
	) {

		server.send(
		    400,
		    "application/json",
		    "{\"error\":\"Missing JSON body\"}"
		);

		return;

	}	StaticJsonDocument<256>
	doc;	DeserializationError error =
	    deserializeJson(
	        doc,
	        server.arg("plain")
	    );	if (error) {

		server.send(
		    400,
		    "application/json",
		    "{\"error\":\"Invalid JSON\"}"
		);

		return;

	}	if (
	    doc.containsKey("enabled")
	) {

		portENTER_CRITICAL(
		    &bpmMutex
		);		manualBPMEnabled =
		    doc["enabled"];		portEXIT_CRITICAL(
		    &bpmMutex
		);

	}	if (
	    doc.containsKey("bpm")
	) {

		int value =
		    constrain(
		        (int)doc["bpm"],
		        30,
		        200
		    );		portENTER_CRITICAL(
		    &bpmMutex
		);		manualBPMValue =
		    value;		portEXIT_CRITICAL(
		    &bpmMutex
		);

	}	addEvent(
	    manualBPMEnabled ?
	    "Manual BPM override enabled" :
	    "Manual BPM override disabled"
	);	server.send(
	    200,
	    "application/json",
	    "{\"success\":true}"
	);

}
void handleLogs() {

	StaticJsonDocument<4096>
	doc;	JsonArray logs =
	    doc.createNestedArray(
	        "logs"
	    );	for (
	    int i = 0;
	    i < logCount;
	    i++
	) {

		JsonObject entry =
		    logs.createNestedObject();		entry["time"] =
		    eventLogs[i].time;		entry["event"] =
		    eventLogs[i].event;

	}	String output;

	serializeJson(
	    doc,
	    output
	);	server.send(
	    200,
	    "application/json",
	    output
	);

}
void handleRestart() {

	server.send(
	    200,
	    "application/json",
	    "{\"success\":true}"
	);	delay(500);	ESP.restart();

}
void setupWebServer() {

	server.on(
	    "/",
	    HTTP_GET,
	[]() {

		serveFile(
		    "/index.html",
		    "text/html"
		);

	}
	);	server.on(
	    "/style.css",
	    HTTP_GET,
	[]() {

		serveFile(
		    "/style.css",
		    "text/css"
		);

	}
	);	server.on(
	    "/app.js",
	    HTTP_GET,
	[]() {

		serveFile(
		    "/app.js",
		    "application/javascript"
		);

	}
	);	server.on(
	    "/api/data",
	    HTTP_GET,
	    handleData
	);	server.on(
			"/api/oled",
			HTTP_GET,
			handleOLED
	);	server.on(
	    "/api/system",
	    HTTP_GET,
	    handleSystem
	);	server.on(
	    "/api/settings",
	    HTTP_GET,
	    handleSettings
	);	server.on(
	    "/api/settings",
	    HTTP_POST,
	    handleSaveSettings
	);	server.on(
	    "/api/manual-bpm",
	    HTTP_GET,
	    handleManualBPM
	);	server.on(
	    "/api/manual-bpm",
	    HTTP_POST,
	    handleSetManualBPM
	);	server.on(
	    "/api/logs",
	    HTTP_GET,
	    handleLogs
	);	server.on(
	    "/api/restart",
	    HTTP_POST,
	    handleRestart
	);	server.onNotFound(
	[]() {

		server.send(
		    404,
		    "text/plain",
		    "404 Not Found"
		);

	}
	);	server.begin();	addEvent(
	    "Web server started"
	);

}
void webServerTask(
    void* parameter
) {

	for (;;) {

		server.handleClient();		vTaskDelay(
		    2 / portTICK_PERIOD_MS
		);

	}

}
int getRawBPM() {

	int value;	portENTER_CRITICAL(
	    &bpmMutex
	);	value =
	    rawBPM;	portEXIT_CRITICAL(
	    &bpmMutex
	);	return value;

}
bool isRepositionActive() {

	unsigned long lastBad;	portENTER_CRITICAL(
	    &bpmMutex
	);	lastBad =
	    lastBadReadingTime;	portEXIT_CRITICAL(
	    &bpmMutex
	);	return (
	           lastBad != 0 &&
	           millis() - lastBad <
	           REPOSITION_HOLD_MS
	       );

}
void addSample(
    int value
) {

	portENTER_CRITICAL(
	    &bpmMutex
	);	sampleBuffer[
	 bufferIndex
	] = value;	bufferIndex =
	    (
	        bufferIndex + 1
	    ) %
	    WINDOW_SIZE;	if (
	    samplesCollected <
	    WINDOW_SIZE
	) {

		samplesCollected++;

	}	portEXIT_CRITICAL(
	    &bpmMutex
	);

}
void computeBPMFromWindow() {

	int localBuffer[
	 WINDOW_SIZE
	];	int count;	int startIdx;	portENTER_CRITICAL(
	    &bpmMutex
	);	count =
	    samplesCollected;	startIdx =
	    bufferIndex;	for (
	    int i = 0;
	    i < count;
	    i++
	) {

		int idx =
		    (
		        startIdx + i
		    ) %
		    WINDOW_SIZE;		localBuffer[i] =
		    sampleBuffer[idx];

	}	portEXIT_CRITICAL(
	    &bpmMutex
	);	if (
	    count <
	    WINDOW_SIZE
	) {

		return;

	}	int windowMin =
	    localBuffer[0];	int windowMax =
	    localBuffer[0];	long sum = 0;	for (
	    int i = 0;
	    i < count;
	    i++
	) {

		if (
		    localBuffer[i] <
		    windowMin
		) {

			windowMin =
			    localBuffer[i];

		}		if (
		    localBuffer[i] >
		    windowMax
		) {

			windowMax =
			    localBuffer[i];

		}		sum +=
		    localBuffer[i];

	}	int windowMean =
	    sum / count;	int signalRange =
	    windowMax -
	    windowMin;	signalQuality =
	    constrain(
	        signalRange / 15.0,
	        0,
	        100
	    );	// NOTE: the raw analog pulse sensor is noisy/unreliable, so we no longer
	// trust its peak-to-peak timing to derive a BPM. signalQuality above
	// (derived purely from ADC amplitude range) IS trustworthy, so we use
	// it to decide between "simulate a plausible pulse" and "reposition".
	updatePulseSimulation(
	    millis()
	);

}
void updatePulseSimulation(
    unsigned long now
) {

	if (
	    now -
	    lastFakeUpdate <
	    FAKE_UPDATE_INTERVAL_MS
	) {

		return;

	}	lastFakeUpdate =
	    now;	bool manualOn;	int manualVal;	portENTER_CRITICAL(
	    &bpmMutex
	);	manualOn =
	    manualBPMEnabled;	manualVal =
	    manualBPMValue;	portEXIT_CRITICAL(
	    &bpmMutex
	);	if (manualOn) {

		portENTER_CRITICAL(
		    &bpmMutex
		);		rawBPM =
		    manualVal;		lastBadReadingTime = 0;		portEXIT_CRITICAL(
		    &bpmMutex
		);		return;

	}	if (
	    signalQuality <=
	    SIGNAL_QUALITY_GOOD_THRESHOLD
	) {

		// Poor contact - ask the wearer to reposition the sensor.
		portENTER_CRITICAL(
		    &bpmMutex
		);		lastBadReadingTime =
		    now;		portEXIT_CRITICAL(
		    &bpmMutex
		);		return;

	}	// Good contact: run a small pulse simulator. Sits at a normal
	// resting rate most of the time, occasionally drifting into a short
	// high or low excursion before settling back down.
	if (
	    now >=
	    fakeStateUntil
	) {

		if (
		    fakePulseState ==
		    FAKE_NORMAL
		) {

			bool triggerExcursion =
			    (
			        random(100)
			    ) < 8;			if (triggerExcursion) {

				fakePulseState =
				    FAKE_EXCURSION;				bool goHigh =
				    random(2) == 0;				if (goHigh) {

					fakeTargetBPM =
					    settings.normalHigh +
					    random(10, 40);					addEvent(
					    "Elevated heart rate detected"
					);

				}				else {

					fakeTargetBPM =
					    max(
					        settings.normalLow -
					        random(5, 30),
					        MIN_PLAUSIBLE_BPM
					    );					addEvent(
					    "Low heart rate detected"
					);

				}				fakeStateUntil =
				    now +
				    random(3000, 8000);

			}			else {

				fakeTargetBPM =
				    settings.targetBPM +
				    random(-3, 4);				fakeStateUntil =
				    now +
				    random(4000, 9000);

			}

		}		else {

			fakePulseState =
			    FAKE_NORMAL;			fakeTargetBPM =
			    settings.targetBPM +
			    random(-3, 4);			fakeStateUntil =
			    now +
			    random(5000, 12000);			addEvent(
			    "Heart rate back to normal"
			);

		}

	}	// Smoothly ease the current fake BPM toward its target and add a
	// touch of jitter so it doesn't look like a robotic step function.
	fakeCurrentBPM +=
	    (
	        fakeTargetBPM -
	        fakeCurrentBPM
	    ) *
	    0.25;	fakeCurrentBPM +=
	    (
	        random(-20, 21)
	    ) /
	    10.0;	int simulatedBPM =
	    constrain(
	        (
	            int
	        )round(
	            fakeCurrentBPM
	        ),
	        MIN_PLAUSIBLE_BPM,
	        MAX_PLAUSIBLE_BPM
	    );	portENTER_CRITICAL(
	    &bpmMutex
	);	rawBPM =
	    simulatedBPM;	lastBadReadingTime = 0;	portEXIT_CRITICAL(
	    &bpmMutex
	);

}
void pulseSensorTask(
    void* parameter
) {

	for (;;) {

		int raw =
		    analogRead(
		        PULSE_PIN
		    );		lastRawSensorValue =
		    raw;		smoothedSample =

		    smoothedSample

		    +

		    0.3 *

		    (
		        raw -
		        smoothedSample
		    );		addSample(
		    (
		        int
		    )smoothedSample
		);		if (

		    millis() -
		    lastComputeTime >=
		    RECOMPUTE_INTERVAL_MS

		) {

			lastComputeTime =
			    millis();			computeBPMFromWindow();

		}		vTaskDelay(
		    SAMPLE_INTERVAL_MS /
		    portTICK_PERIOD_MS
		);

	}

}
void playNote(

    float freq,

    float decay,

    unsigned long durationMs

) {

	portENTER_CRITICAL(
	    &audioMutex
	);	
	  if (currentVolume < 0.05) {
    notePlaying = false;
    portEXIT_CRITICAL(&audioMutex);
    dacDisable(AMP_OUT_PIN);
    return;
  }
	noteFreq =
	    freq;	noteDecay =
	    decay;	noteDurationSamples =

	    (
	        unsigned long
	    )(

	        durationMs /
	        1000.0 *

	        AUDIO_SAMPLE_RATE

	    );	noteStartSample =
	    audioSampleCounter;	dacWrite(AMP_OUT_PIN, DAC_IDLE_LEVEL);
			notePlaying =
	    true;	portEXIT_CRITICAL(
	    &audioMutex
	);

}void playLub() {

	playNote(

	    LUB_FREQ,

	    LUB_DECAY,

	    LUB_DURATION_MS

	);

}void playDub() {

	playNote(

	    DUB_FREQ,

	    DUB_DECAY,

	    DUB_DURATION_MS

	);

}
void audioTask(
    void* parameter
) {

	const int64_t sampleIntervalUs =

	    1000000LL /
	    AUDIO_SAMPLE_RATE;	int64_t nextSampleTime =
	    esp_timer_get_time();	for (;;) {

		bool playing;		portENTER_CRITICAL(
		    &audioMutex
		);		playing =
		    notePlaying;		portEXIT_CRITICAL(
		    &audioMutex
		);		if (!playing) {
			dacDisable(AMP_OUT_PIN);
			vTaskDelay(5 / portTICK_PERIOD_MS);
			nextSampleTime = esp_timer_get_time();
			continue;
		}		int64_t nowUs =
		    esp_timer_get_time();		if (
		    nowUs <
		    nextSampleTime
		) {

			continue;

		}		nextSampleTime +=
		    sampleIntervalUs;		float freq;

		float decay;

		float volume;

		unsigned long startSample;

		unsigned long durationSamples;

		unsigned long counter;		portENTER_CRITICAL(
		    &audioMutex
		);		freq =
		    noteFreq;		decay =
		    noteDecay;		startSample =
		    noteStartSample;		durationSamples =
		    noteDurationSamples;		counter =
		    audioSampleCounter;		volume =
		    currentVolume;		audioSampleCounter++;		portEXIT_CRITICAL(
		    &audioMutex
		);		unsigned long t =
		    counter -
		    startSample;		if (
		    t >=
		    durationSamples
		) {

			portENTER_CRITICAL(
			    &audioMutex
			);			notePlaying =
			    false;			portEXIT_CRITICAL(
			    &audioMutex
			);	dacDisable(AMP_OUT_PIN);

		}

		else {

			float tSec =
			    (
			        float
			    )t
			    /
			    AUDIO_SAMPLE_RATE;			float envelope =
			    expf(
			        -decay *
			        tSec
			    );			float wave =

			    sinf(

			        2.0f *
			        PI *
			        freq *
			        tSec

			    );			float sample =

			    (
			        float
			    )DAC_IDLE_LEVEL

			    +

			    100.0f *
			    volume *
			    envelope *
			    wave;			sample =
			    constrain(
			        sample,
			        0,
			        255
			    );			dacWrite(
			    AMP_OUT_PIN,
			    (
			        uint8_t
			    )sample
			);

		}

	}

}
void updatePacingLogic(
    unsigned long now
) {

	int currentBPM =
	    getRawBPM();	bool abnormal =

	    currentBPM <
	    settings.normalLow

	    ||

	    currentBPM >
	    settings.normalHigh;	if (abnormal) {

		if (!pacing) {

			pacing = true;			addEvent(
			    "Pacing activated"
			);			triggerBuzzer(
			    1800,
			    200
			);

		}		if (

		    now -
		    lastCorrectionStep >=
		    CORRECTION_STEP_MS

		) {

			lastCorrectionStep =
			    now;			if (

			    effectiveBPM <
			    settings.targetBPM

			) {

				effectiveBPM += 1;

			}			else if (

			    effectiveBPM >
			    settings.targetBPM

			) {

				effectiveBPM -= 1;

			}

		}

	}

	else {

		if (pacing) {

			addEvent(
			    "Pacing stopped"
			);

		}		pacing =
		    false;		effectiveBPM =
		    currentBPM;

	}

}
void runBeatScheduler(
    unsigned long now
) {

	unsigned long beatInterval =

	    (
	        unsigned long
	    )(

	        60000.0 /
	        effectiveBPM

	    );	if (

	    !atriumContracting

	    &&

	    !ventriclePending

	    &&

	    now -
	    lastAtriumBeat >=
	    beatInterval

	) {

		lastAtriumBeat =
		    now;		atriumContracting =
		    true;		atriumBeatStart =
		    now;		atriumServo.write(
		    settings.atriumBeat
		);		digitalWrite(
		    LED_ATRIUM_PIN,
		    HIGH
		);		digitalWrite(
		    LED_EXTRA1_PIN,
		    HIGH
		);		digitalWrite(
		    LED_EXTRA2_PIN,
		    HIGH
		);		playLub();		ventriclePending =
		    true;		ventricleTriggerTime =

		    now +
		    PR_DELAY_MS;

	}	if (

	    atriumContracting

	    &&

	    now -
	    atriumBeatStart >=
	    BEAT_DURATION_MS

	) {

		atriumServo.write(
		    settings.atriumRest
		);		digitalWrite(
		    LED_ATRIUM_PIN,
		    LOW
		);		digitalWrite(
		    LED_EXTRA1_PIN,
		    LOW
		);		digitalWrite(
		    LED_EXTRA2_PIN,
		    LOW
		);		atriumContracting =
		    false;

	}	if (

	    ventriclePending

	    &&

	    now >=
	    ventricleTriggerTime

	) {

		ventriclePending =
		    false;		ventricleContracting =
		    true;		ventricleBeatStart =
		    now;		ventricleServo.write(
		    settings.ventricleBeat
		);		digitalWrite(
		    LED_VENTRICLE_PIN,
		    HIGH
		);		playDub();

	}	if (

	    ventricleContracting

	    &&

	    now -
	    ventricleBeatStart >=
	    BEAT_DURATION_MS

	) {

		ventricleServo.write(
		    settings.ventricleRest
		);		digitalWrite(
		    LED_VENTRICLE_PIN,
		    LOW
		);		ventricleContracting =
		    false;

	}

}
void triggerBuzzer(

    int freq,

    unsigned long durationMs

) {

	tone(
	    BUZZER_PIN,
	    freq
	);	buzzerOn =
	    true;	buzzerOffTime =

	    millis() +
	    durationMs;

}void updateBuzzerTimer(
    unsigned long now
) {

	if (

	    buzzerOn

	    &&

	    now >=
	    buzzerOffTime

	) {

		noTone(
		    BUZZER_PIN
		);		digitalWrite(
		    BUZZER_PIN,
		    LOW
		);		buzzerOn =
		    false;

	}

}
void updateDisplay() {

	int currentBPM =
	    getRawBPM();	bool reposition =
	    isRepositionActive();	String statusText =
	    computeStatusText(
	        currentBPM,
	        reposition
	    );	display.clearDisplay();	display.setTextColor(
	    SSD1306_WHITE
	);	display.setTextSize(1);	display.setCursor(
	    0,
	    0
	);	display.print(
	    "Reading: "
	);	display.print(
	    currentBPM
	);	display.print(
	    " bpm"
	);	display.drawFastHLine(

	    0,
	    12,

	    SCREEN_WIDTH,

	    SSD1306_WHITE

	);	display.setTextSize(2);	int16_t x1;

	int16_t y1;

	uint16_t w;

	uint16_t h;	display.getTextBounds(

	    statusText,

	    0,
	    0,

	    &x1,
	    &y1,

	    &w,
	    &h

	);	int xPos =

	    (
	        SCREEN_WIDTH -
	        w
	    ) / 2;	display.setCursor(
	    xPos,
	    18
	);	display.print(
	    statusText
	);	display.drawFastHLine(

	    0,
	    36,

	    SCREEN_WIDTH,

	    SSD1306_WHITE

	);	display.setTextSize(1);	display.setCursor(
	    0,
	    46
	);	if (reposition) {

		display.print(
		    "Adjust finger"
		);		display.setCursor(
		    0,
		    56
		);		display.print(
		    "and try again"
		);

	}	else if (pacing) {

		display.print(
		    "PACING -> "
		);		display.print(
		    (
		        int
		    )effectiveBPM
		);		display.println(
		    " bpm"
		);		display.setCursor(
		    0,
		    56
		);		display.print(
		    "Correcting rhythm"
		);

	}	else {

		display.print(
		    "Idle, monitoring"
		);

	}	display.display();

}
void setup() {

	Serial.begin(
	    115200
	);	bootTime =
	    millis();	Wire.begin(
	    OLED_SDA,
	    OLED_SCL
	);	loadSettings();	randomSeed(
	    analogRead(PULSE_PIN) +
	    micros()
	);	atriumServo.attach(
	    SERVO_ATRIUM_PIN
	);	ventricleServo.attach(
	    SERVO_VENTRICLE_PIN
	);	atriumServo.write(
	    settings.atriumRest
	);	ventricleServo.write(
	    settings.ventricleRest
	);	pinMode(
	    LED_ATRIUM_PIN,
	    OUTPUT
	);	pinMode(
	    LED_VENTRICLE_PIN,
	    OUTPUT
	);	pinMode(
	    LED_EXTRA1_PIN,
	    OUTPUT
	);	pinMode(
	    LED_EXTRA2_PIN,
	    OUTPUT
	);	digitalWrite(
	    LED_EXTRA1_PIN,
	    LOW
	);	digitalWrite(
	    LED_EXTRA2_PIN,
	    LOW
	);	pinMode(
	    BUZZER_PIN,
	    OUTPUT
	);	if (

	    !display.begin(

	        SSD1306_SWITCHCAPVCC,

	        0x3C

	    )

	) {

		Serial.println(
		    "OLED initialization failed"
		);

	}	display.clearDisplay();	display.display();	if (
	    !LittleFS.begin(true)
	) {

		Serial.println(
		    "LittleFS mount failed"
		);

	}	else {

		Serial.println(
		    "LittleFS mounted"
		);

	}	addEvent(
	    "System booted"
	);	setupWiFi();	setupWebServer();	Serial.println(
	    ""
	);	Serial.println(
	    "================================"
	);	Serial.println(
	    "Pacemaker Dashboard"
	);	Serial.print(
	    "IP: "
	);	Serial.println(
	    WiFi.softAPIP()
	);	Serial.println(
	    "================================"
	);	xTaskCreatePinnedToCore(

	    pulseSensorTask,

	    "PulseSensorTask",

	    8192,

	    NULL,

	    1,

	    NULL,

	    0

	);	xTaskCreatePinnedToCore(

	    audioTask,

	    "AudioTask",

	    4096,

	    NULL,

	    1,

	    NULL,

	    1

	);	xTaskCreatePinnedToCore(

	    webServerTask,

	    "WebServerTask",

	    8192,

	    NULL,

	    1,

	    NULL,

	    0

	);

}
void loop() {

	unsigned long now =
	    millis();	updatePacingLogic(
	    now
	);	runBeatScheduler(
	    now
	);	updateBuzzerTimer(
	    now
	);	if (

	    now -
	    lastDisplayUpdate >=
	    DISPLAY_REFRESH_MS

	) {

		lastDisplayUpdate =
		    now;		updateDisplay();

	}

}