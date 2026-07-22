# Node-RED + Influx for Thread sensors

## Topics

See `../shared/protocol/mqtt_schema.md`.

Subscribe: `home/#`

## Import

1. Node-RED → menu → Import → select `flows_thread_sensors.json`
2. Configure the MQTT broker node to your Mosquitto container
3. Optional: set InfluxDB server credentials in the Influx out nodes
4. Deploy

## What the flow does

- Logs hub registry / events
- Parses `.../state` JSON for `temp_hum` and `contact`
- Writes measurements `temp_hum` and `contact` to Influx (if configured)
- Example: contact `open` → debug warning

## Grafana

Create panels from:

```
from(bucket: "iot")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "temp_hum")
```

Tags: `device_id`, `room`, `name`.
