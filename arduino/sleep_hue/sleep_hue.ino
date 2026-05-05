// Sleep-Hue: press the QT Py BOOT button to start the bedtime wind-down.
// Targets the Adafruit QT Py ESP32 family (S2 / S3 / Pico / C3).

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"

#ifndef BUTTON_PIN
  #if CONFIG_IDF_TARGET_ESP32C3
    #define BUTTON_PIN 9
  #else
    #define BUTTON_PIN 0
  #endif
#endif

Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static void led(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

static bool recallScene(const char* sceneId, uint32_t durationMs) {
  WiFiClientSecure client;
  client.setInsecure();  // Hue bridge serves a self-signed cert
  HTTPClient http;
  String url = String("https://") + HUE_BRIDGE_IP + "/clip/v2/resource/scene/" + sceneId;
  http.begin(client, url);
  http.addHeader("hue-application-key", HUE_APP_KEY);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"recall\":{\"action\":\"active\",\"duration\":") + durationMs + "}}";
  int code = http.PUT(body);
  Serial.printf("PUT scene -> HTTP %d\n", code);
  http.end();
  return code >= 200 && code < 300;
}

static void windDown() {
  Serial.println("wind-down triggered");
  led(40, 0, 60);
  bool ok = recallScene(SLEEPY_SCENE_ID, WIND_DOWN_MS);
  for (int i = 0; i < 4; i++) {
    led(0, ok ? 50 : 0, ok ? 0 : 50);
    delay(150);
    led(0, 0, 0);
    delay(150);
  }
  led(0, 8, 0);
}

static void connectWiFi() {
  led(0, 0, 40);
  Serial.printf("connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.printf("\nconnected: %s\n", WiFi.localIP().toString().c_str());
  led(0, 8, 0);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  #ifdef NEOPIXEL_POWER
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
  #endif
  pixel.begin();
  pixel.setBrightness(50);

  delay(500);
  Serial.println("\nSleep-Hue starting");
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    led(40, 0, 0);
    connectWiFi();
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(40);
    if (digitalRead(BUTTON_PIN) == LOW) {
      windDown();
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }
  delay(20);
}
