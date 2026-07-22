# MQTT schema (v1)

Broker: mini PC Mosquitto (LAN). Hub and sensors are publishers; Node-RED subscribes.

## Hub

| Topic | Retain | Payload |
|-------|--------|---------|
| `home/hub/<hub_id>/status` | yes | `online` / `offline` (LWT) |
| `home/hub/<hub_id>/info` | yes | `{"ip":"...","fw":"...","devices":N,"pair_open":false}` |
| `home/hub/<hub_id>/registry` | yes | JSON array of registry entries |
| `home/hub/<hub_id>/event` | no | pairing / online events |

Registry entry:

```json
{
  "id": "th-a1b2c3",
  "type": "temp_hum",
  "name": "Kitchen TH",
  "room": "Kitchen",
  "fw": "0.1.0",
  "last_seen": 1710000000,
  "online": true
}
```

## Devices

Pattern: `home/<room_slug>/<device_id>/<leaf>`

| Leaf | Retain | Notes |
|------|--------|-------|
| `status` | yes | `online`/`offline` + LWT |
| `meta` | yes | static-ish identity |
| `state` | yes | latest reading |
| `set/*` | no | commands (optional) |

`room_slug`: lowercase, spaces → `_`. Empty room → `unassigned`.

### meta

```json
{"type":"temp_hum","name":"Kitchen TH","room":"Kitchen","fw":"0.1.0","product":"Thread_TempHum"}
```

### state — temp_hum

```json
{"type":"temp_hum","t":22.4,"h":48.1,"bat":87,"rssi":-61,"ts":1710000000}
```

### state — contact

```json
{"type":"contact","contact":"open","bat":90,"rssi":-70,"ts":1710000000}
```

`contact` is `open` or `closed`.

## Node-RED

Subscribe: `home/#`  
Filter hub vs devices; write Influx measurements `temp_hum`, `contact`.
