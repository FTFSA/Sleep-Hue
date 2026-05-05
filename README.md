# Sleep-Hue

Adafruit QT Py ESP32 firmware that toggles a Philips Hue bedroom light from a
single button, with a 5-minute "sleep" fade on long-press.

## Behavior

| Input | Result |
| --- | --- |
| Short press | Toggle light at **4500 K, 50%** |
| Hold ≥ 2 s | Enter **sleep mode**: 2200 K, 20% → 10% → 5% → 1% → 1% → off (1 min per step) |
| Any press during sleep mode | Light off, exit sleep mode |

## Wiring (2-pin normally-open button)

```
   QT Py ESP32                   Button
   ┌──────────┐                  ┌──────┐
   │   A0  ───┼──────────────────┤      │
   │          │                  │      │
   │   GND ───┼──────────────────┤      │
   └──────────┘                  └──────┘
```

- One leg of the button → **A0** (`BUTTON_PIN` in the sketch).
- Other leg of the button → **GND**.
- The pin uses `INPUT_PULLUP`, so no external resistor is needed. Pressed = LOW.
- The two legs are interchangeable on a normally-open SPST button.

`A0` resolves to the correct GPIO on every QT Py ESP32 variant
(ESP32 Pico = GPIO26, ESP32-S2/S3 = GPIO18, ESP32-C3 = GPIO4). To use a
different pin, change `BUTTON_PIN` in `Sleep-Hue.ino`.

## One-time Hue bridge setup

1. Press the round button on top of the Hue bridge.
2. Within 30 s, run from any machine on the same LAN:
   ```sh
   curl -X POST http://192.168.1.68/api -H 'Content-Type: application/json' \
        -d '{"devicetype":"sleep-hue#qtpy"}'
   ```
   The response contains `{"success":{"username":"<long-string>"}}`. Copy that
   string into `HUE_USERNAME` in `Sleep-Hue.ino`.
3. List your lights to find the bedroom light's id:
   ```sh
   curl http://192.168.1.68/api/<username>/lights | jq
   ```
   Set `HUE_LIGHT_ID` accordingly.

## Build / flash

Arduino IDE with the **esp32** board package (Espressif Systems). Select your
QT Py variant under *Tools → Board → ESP32 Arduino*, then upload `Sleep-Hue.ino`.
No extra libraries are required — `WiFi.h` and `HTTPClient.h` ship with the core.

Before uploading, fill in `WIFI_SSID`, `WIFI_PASSWORD`, `HUE_USERNAME`, and
`HUE_LIGHT_ID` at the top of the sketch.
