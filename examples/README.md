# Atomic examples (V0)

Talk to one module over MQTT or USB serial. LEDs: `green`, `red`, `yellow`, `blue`.

## MQTT

Topic: `atomic/{deviceId}/cmd`

`deviceId` is the Wi-Fi MAC as 12 lowercase hex digits. Boot serial at 115200 prints `Device` plus that id.

Publish with `-h test.mosquitto.org` to the same topic `atomic/80f3daad99e0/cmd` (device id from serial; that id was printed before).

```
mosquitto_pub -h test.mosquitto.org -t 'atomic/80f3daad99e0/cmd' -f examples/mqtt/led.json
```

Same for `stop.json` and `sequence.json`. Each sequence step is the full LED state; omitted LEDs are off. Repeat 1–20, 1–50 steps, 50–10000 ms each. `sequence-off.json` is one all-off step.

## Serial

115200 baud, one JSON line, newline-terminated. `id` is echoed in the reply.

Paste a line from `examples/serial/` (`set_led`, `stop_sequence`, `run_sequence`). Serial sequence steps use `duration_ms` and `leds` as an array of names.
