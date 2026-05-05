// Sleep-Hue: Adafruit QT Py ESP32 + Philips Hue button controller.
//
// Short press: toggle light at 4500K, 50% brightness.
// Long press (>= 2s): start sleep fade — 2200K, stepping
//   20% -> 10% -> 5% -> 1% -> 1% -> off, one minute per step.
// Any press during the sleep fade turns the light off immediately.

#include <WiFi.h>
#include <HTTPClient.h>

// ---- User configuration ------------------------------------------------
static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

static const char* HUE_BRIDGE_IP = "192.168.1.68";
static const char* HUE_USERNAME  = "YOUR_HUE_API_USERNAME"; // see README
static const int   HUE_LIGHT_ID  = 1;                       // bedroom light id

// ---- Pins --------------------------------------------------------------
// Button is 2-pin, normally open: one leg to BUTTON_PIN, other leg to GND.
// INPUT_PULLUP means the pin reads HIGH when released, LOW when pressed.
static const int BUTTON_PIN = A0;

// ---- Behavior tuning ---------------------------------------------------
static const unsigned long DEBOUNCE_MS    = 30;
static const unsigned long LONG_PRESS_MS  = 2000;
static const unsigned long SLEEP_STEP_MS  = 60UL * 1000UL;

// Hue color-temperature is in mireds (1,000,000 / kelvin).
static const int CT_4500K = 1000000 / 4500; // 222
static const int CT_2200K = 1000000 / 2200; // 454

// Hue brightness is 1..254. Convert percent -> bri, with 1% floor at 3.
static int pctToBri(int pct) {
  if (pct <= 0) return 0;
  int bri = (pct * 254) / 100;
  if (bri < 1) bri = 1;
  if (bri > 254) bri = 254;
  return bri;
}

// Sleep fade: each step is one minute. Final entry is "off".
struct SleepStep { int pct; bool off; };
static const SleepStep SLEEP_STEPS[] = {
  { 20, false },
  { 10, false },
  {  5, false },
  {  1, false },
  {  1, false },
  {  0, true  },
};
static const int SLEEP_STEP_COUNT = sizeof(SLEEP_STEPS) / sizeof(SLEEP_STEPS[0]);

// ---- State -------------------------------------------------------------
enum Mode { MODE_IDLE, MODE_SLEEP };
static Mode  mode          = MODE_IDLE;
static bool  lightOn       = false;
static int   sleepStep     = 0;
static unsigned long sleepStepStart = 0;

// Button tracking
static int           btnStable     = HIGH;
static int           btnLastRead   = HIGH;
static unsigned long btnLastChange = 0;
static unsigned long btnPressStart = 0;
static bool          longFired     = false;

// ---- Hue HTTP ----------------------------------------------------------
static bool hueSend(const String& body) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + HUE_BRIDGE_IP +
               "/api/" + HUE_USERNAME +
               "/lights/" + HUE_LIGHT_ID + "/state";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest("PUT", (uint8_t*)body.c_str(), body.length());
  Serial.printf("PUT %s -> %d : %s\n", url.c_str(), code, body.c_str());
  http.end();
  return code > 0 && code < 400;
}

static void hueOff() {
  hueSend("{\"on\":false}");
  lightOn = false;
}

static void hueNormalOn() {
  String body = String("{\"on\":true,\"bri\":") + pctToBri(50) +
                ",\"ct\":" + CT_4500K + ",\"transitiontime\":2}";
  hueSend(body);
  lightOn = true;
}

static void hueSleepStep(const SleepStep& s, bool firstStep) {
  if (s.off) {
    hueOff();
    return;
  }
  String body = String("{\"on\":true,\"bri\":") + pctToBri(s.pct) +
                ",\"ct\":" + CT_2200K +
                // Smooth fade between steps; faster on the very first.
                ",\"transitiontime\":" + (firstStep ? 10 : 50) + "}";
  hueSend(body);
  lightOn = true;
}

// ---- Actions -----------------------------------------------------------
static void onShortPress() {
  if (mode == MODE_SLEEP) {
    Serial.println("Short press during sleep -> OFF");
    mode = MODE_IDLE;
    hueOff();
    return;
  }
  if (lightOn) {
    Serial.println("Short press -> OFF");
    hueOff();
  } else {
    Serial.println("Short press -> ON 4500K/50%");
    hueNormalOn();
  }
}

static void onLongPress() {
  if (mode == MODE_SLEEP) {
    // Treat a long press during sleep the same as any other press.
    Serial.println("Long press during sleep -> OFF");
    mode = MODE_IDLE;
    hueOff();
    return;
  }
  Serial.println("Long press -> SLEEP MODE");
  mode = MODE_SLEEP;
  sleepStep = 0;
  sleepStepStart = millis();
  hueSleepStep(SLEEP_STEPS[0], true);
}

// ---- Button polling (non-blocking, debounced) --------------------------
static void pollButton() {
  int raw = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (raw != btnLastRead) {
    btnLastRead   = raw;
    btnLastChange = now;
  }
  if ((now - btnLastChange) < DEBOUNCE_MS) return;
  if (raw == btnStable) {
    // While held, fire long press as soon as the threshold is crossed.
    if (btnStable == LOW && !longFired &&
        (now - btnPressStart) >= LONG_PRESS_MS) {
      longFired = true;
      onLongPress();
    }
    return;
  }

  btnStable = raw;
  if (btnStable == LOW) {
    btnPressStart = now;
    longFired = false;
  } else {
    // Released: short press only if the long-press handler didn't already fire.
    if (!longFired) onShortPress();
  }
}

// ---- Sleep-mode tick ---------------------------------------------------
static void tickSleep() {
  if (mode != MODE_SLEEP) return;
  if ((millis() - sleepStepStart) < SLEEP_STEP_MS) return;

  sleepStep++;
  sleepStepStart = millis();
  if (sleepStep >= SLEEP_STEP_COUNT) {
    mode = MODE_IDLE;
    hueOff();
    return;
  }
  hueSleepStep(SLEEP_STEPS[sleepStep], false);
  if (SLEEP_STEPS[sleepStep].off) {
    mode = MODE_IDLE;
  }
}

// ---- Setup / loop ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED — will keep retrying in background");
  }
}

void loop() {
  pollButton();
  tickSleep();
}
