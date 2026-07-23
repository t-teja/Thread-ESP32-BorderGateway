# Architecture

## Components

### Border Gateway (ESP32-S3 + ESP32-H2 RCP)

- **Captive portal SoftAP** for first-time Wi-Fi + MQTT configuration (stored in NVS)
- **Wi-Fi STA** backbone to LAN
- **OpenThread Border Router** (Thread mesh ↔ IPv6/Wi-Fi); NAT64/routing come from OTBR but
  aren't needed by sensors — they only ever talk CoAP to the hub's Thread address
- **BLE GATT provisioner** (central) during pairing windows
- **HTTP dashboard** for devices, pairing, rename/location
- **NVS device registry** (id, type, name, room, last seen)
- **CoAP bridge** (Thread ↔ MQTT: sensor reports in, commands out — see below)
- **MQTT bridge** — the hub is the *only* MQTT client in the system; sensors never touch the broker

The on-board ESP32-H2 runs RCP firmware (Spinel UART). Application code runs on the S3 only.

### Sensors (ESP32-H2)

- OpenThread end device (MED or sleepy SED)
- BLE peripheral in pairing mode only
- After pair: Thread-only — sensor talks CoAP to the hub (no MQTT client on the sensor)
- NVS: device_id, name, room, hub's Thread address, Thread dataset blob

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
 "name":"Kitchen TH","room":"Kitchen","type":"temp_hum",
 "hub_addr":"fd00:db8:a0:0:...:<hub mesh-local EID>"}
{"cmd":"unpair"}
```

Sensor → hub `Info` advertisement / read:

```json
{"mac":"AABBCCDDEEFF","type":"temp_hum","fw":"0.1.0","bat":100,"product":"Thread_TempHum"}
```

On successful provision the sensor stores NVS, disables BLE, attaches Thread, and starts
reporting to `hub_addr` over CoAP (no MQTT credentials are ever given to the sensor).

## Sensor ↔ hub protocol (CoAP over Thread)

The hub runs a CoAP server on `OT_DEFAULT_COAP_PORT` (5683); each sensor does too, for
commands. See `Thread_BorderGateway/src/coap_bridge.c` and
`Thread_TempHum/src/coap_sensor.c`.

| Path | Direction | Payload | Hub action |
|------|-----------|---------|------------|
| `/s` | sensor → hub | `{"id":..,"type":..,"t":..,"h":..,"bat":..,"rssi":..}` (or `"contact"`) | update registry, forward to `home/<room>/<id>/state` |
| `/m` | sensor → hub | `{"id":..,"fw":..,"product":..}` | update registry fw, forward to `home/<room>/<id>/meta` |
| `/e` | sensor → hub | `{"id":..}` (button press) | hub LED identify + `home/hub/<id>/event` |
| `/c` | hub → sensor | `{"cmd":"identify"}` | sensor blinks its LED |

Every inbound sensor request also refreshes the hub's record of that device's Thread
address (used to route `/c` commands) and its `last_seen` timestamp; a device with no
report for `HUB_DEVICE_OFFLINE_TIMEOUT_SEC` is marked offline (`status` → `offline`).

## MQTT schema

```
home/hub/<hub_id>/status          online|offline
home/hub/<hub_id>/registry        JSON array of devices (retain)
home/<room>/<device_id>/status    online|offline (hub-managed timeout, retain)
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
- Sensor↔hub CoAP traffic never leaves the Thread mesh (network-key secured); only the
  hub speaks MQTT/IP to the broker
- Harden later: pair token, Wi-Fi AP isolation, MQTT TLS, dataset rotation

## Why not Matter (for now)

Matter needs a Linux controller and heavier commissioning. DIY sensors use this lighter BLE+Thread(CoAP)+MQTT(hub-only) path. Matter can be added later as a second intake into the same MQTT bus for commercial devices (e.g. Panasonic AC).
