# Architecture

## Components

### Border Gateway (ESP32-S3 + ESP32-H2 RCP)

- **Captive portal SoftAP** for first-time Wi-Fi + MQTT configuration (stored in NVS)
- **Wi-Fi STA** backbone to LAN
- **OpenThread Border Router** (Thread mesh ↔ IPv6/Wi-Fi)
- **NAT64** so Thread nodes can reach an IPv4 MQTT broker
- **BLE GATT provisioner** (central) during pairing windows
- **HTTP dashboard** for devices, pairing, rename/location
- **NVS device registry** (id, type, name, room, last seen)
- **MQTT bridge** (hub status + optional telemetry mirror)

The on-board ESP32-H2 runs RCP firmware (Spinel UART). Application code runs on the S3 only.

### Sensors (ESP32-H2)

- OpenThread end device (MED or sleepy SED)
- BLE peripheral in pairing mode only
- After pair: Thread + MQTT client publish state
- NVS: device_id, name, room, broker, Thread dataset blob

### IoT server (Windows mini PC)

- Mosquitto, Node-RED, InfluxDB, Grafana (Docker)
- Source of truth for history/automation is MQTT → Node-RED

## Pairing protocol (BLE)

Service UUID: `0xFFF0` (custom 128-bit in firmware)

| Char | UUID | Direction | Payload |
|------|------|-----------|---------|
| Info | `...fff1` | Sensor → Hub (notify/read) | JSON: mac, type, fw, bat, product |
| Ctrl | `...fff2` | Hub → Sensor (write) | JSON commands |
| Status | `...fff3` | Sensor → Hub (notify) | pair progress / result |

Hub → sensor `Ctrl` commands:

```json
{"cmd":"identify"}
{"cmd":"provision","dataset":"<base64 TLVs>","device_id":"th-a1b2c3",
 "name":"Kitchen TH","room":"Kitchen","mqtt_host":"192.168.1.50",
 "mqtt_port":1883,"mqtt_user":"","mqtt_pass":"","topic_base":"home"}
{"cmd":"unpair"}
```

Sensor → hub `Info` advertisement / read:

```json
{"mac":"AABBCCDDEEFF","type":"temp_hum","fw":"0.1.0","bat":100,"product":"Thread_TempHum"}
```

On successful provision the sensor stores NVS, disables BLE, attaches Thread, connects MQTT.

## MQTT schema

```
home/hub/<hub_id>/status          online|offline
home/hub/<hub_id>/registry        JSON array of devices (retain)
home/<room>/<device_id>/status    online|offline (LWT, retain)
home/<room>/<device_id>/meta      type, name, fw, ... (retain)
home/<room>/<device_id>/state     type-specific JSON (retain)
home/<room>/<device_id>/set/#     commands to device
```

Rooms in topics are slugified (`Living Room` → `living_room`). If room empty: `home/unassigned/<id>/...`.

### State examples

Temp/humidity:

```json
{"type":"temp_hum","t":22.4,"h":48.1,"bat":87,"rssi":-61,"ts":1710000000}
```

Contact:

```json
{"type":"contact","contact":"open","bat":90,"rssi":-70,"ts":1710000000}
```

## Security notes (v1)

- Pairing only while hub window open (default 120 s)
- BLE provision sends Thread dataset — physical proximity assumed (Hue-like)
- Harden later: pair token, Wi-Fi AP isolation, MQTT TLS, dataset rotation

## Why not Matter (for now)

Matter needs a Linux controller and heavier commissioning. DIY sensors use this lighter BLE+Thread+MQTT path. Matter can be added later as a second intake into the same MQTT bus for commercial devices (e.g. Panasonic AC).
