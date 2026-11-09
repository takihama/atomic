# Atomic Wi-Fi and MQTT

Wi-Fi puts each Atomic board on the LAN. MQTT then connects to the host in the `MQTT_HOST` constant. Boards never connect to each other, and this firmware does not run a broker.

The Wi-Fi network name and password are compile-time constants in `src/net/wifi_credentials.h` (`WIFI_SSID` and `WIFI_PASSWORD`). Copy `src/net/wifi_credentials.example.h` to that path; the credentials file is gitignored. `begin()` starts the station and returns. The Arduino stack reconnects if the link drops.

The broker address is a compile-time constant at the top of `src/net/mqtt.cpp`: `MQTT_HOST` and `MQTT_PORT` (1883).

The device id is the full Wi-Fi MAC, twelve lowercase hex digits with no separators. Each board subscribes to `atomic/<id>/cmd`.

If the broker is down, LED sequences keep running. The client tries again after two seconds. The board does not reboot to recover.

Commands are one JSON object. Anything else is logged and ignored.

- `{"cmd":"stop"}` — stop the sequence and turn every LED off
- `{"cmd":"led","led":"green","on":true}` — set one LED. `led` is `green`, `red`, `yellow`, or `blue`. `on` is a boolean. This ends a running sequence and leaves the other LEDs as they are.
- `{"cmd":"sequence","repeat":3,"steps":[{"durationMs":500,"leds":{"red":true}}]}` — run that sequence on the board. Omitted LEDs in a step are off. An invalid sequence is ignored, and one that is already running keeps going.

Pasteable payloads are in `examples/`.

Set `WIFI_SSID` and `WIFI_PASSWORD` in `src/net/wifi_credentials.h`, and `MQTT_HOST` in `src/net/mqtt.cpp`, then flash from the project root with `pio run -t upload`. The serial monitor at 115200 prints `Device` plus the twelve-digit id, then `MQTT connected to host:port` once the station has joined.
