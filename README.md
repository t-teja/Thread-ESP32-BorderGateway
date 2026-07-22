# Thread ESP32 Border Gateway

DIY Thread sensor hub with Hue-style pairing. Runs on the Espressif Thread Border Router board (ESP32-S3 host + ESP32-H2 RCP). Sensors publish over MQTT to Node-RED, InfluxDB, and Grafana. No Home Assistant required.

## Features

- Captive portal for first-time Wi-Fi and MQTT setup (no hardcoded credentials)
- Web dashboard to pair sensors, name rooms, and manage the device registry
- BLE provisioning of network and MQTT settings to DIY ESP32-H2 sensors
- MQTT bridge for hub status, registry, and sensor telemetry
- PlatformIO + ESP-IDF projects for hub, temperature/humidity, and contact sensors

## Project structure

    .
    +-- Thread_BorderGateway/   ESP32-S3 hub firmware
    +-- Thread_TempHum/         ESP32-H2 temperature and humidity sensor
    +-- Thread_Contact/         ESP32-H2 door/window contact sensor
    +-- shared/protocol/        Shared device types and MQTT schema
    +-- nodered/                Example Node-RED flow
    +-- docs/                   Architecture, pairing, flashing notes

## Requirements

- ESP Thread Border Router board (or ESP32-S3 + ESP32-H2 RCP)
- ESP32-H2 boards for sensors
- VS Code + PlatformIO IDE
- LAN MQTT broker (for example Mosquitto) and optional Node-RED stack

## Quick start (hub)

1. Open Thread_BorderGateway in VS Code / PlatformIO.
2. Build and upload to the ESP32-S3 USB port.
3. On first boot (or after factory reset), the hub creates an open SoftAP named ThreadHub-Setup-XXXX.
4. Connect a phone or laptop to that network. Open http://192.168.4.1/ if the captive portal does not appear.
5. Enter home Wi-Fi and MQTT broker details, then save. The hub reboots onto your LAN.
6. Open the dashboard at the hub IP (serial log or router DHCP). Use Add device to pair sensors.

Reconfigure later from Settings on the dashboard, or factory-reset to return to the setup AP.

## Sensors

1. Flash Thread_TempHum or Thread_Contact to an ESP32-H2.
2. On the hub dashboard, tap Add device.
3. Hold the sensor button about 3 seconds (fast LED blink).
4. Select the sensor, set name and room, then Pair.
5. Telemetry appears on MQTT under home/ROOM/DEVICE_ID/...

See docs/PAIRING.md and docs/FLASHING.md.

## MQTT

Topic scheme: shared/protocol/mqtt_schema.md

Import nodered/flows_thread_sensors.json on your IoT host.

## Security

- Credentials are stored in device NVS.
- Setup SoftAP is open by default (physical proximity assumed).
- Do not commit include/secrets.h or real passwords. That file is optional and gitignored; the captive portal is the supported setup path.

## OpenThread

Portal, dashboard, BLE pairing, and MQTT are implemented. Full Thread Border Router radio integration is staged. See docs/OTBR_IDF.md.

## License

Private / project use unless otherwise stated.
