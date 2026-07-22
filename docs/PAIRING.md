# Pairing UX (Hue-style)

## User flow

1. Power the hub (ESP Thread BR board). Note IP from serial or router DHCP (`thread-hub`).
2. Open `http://<hub-ip>/`.
3. Click **Add device** (opens 120 s pairing window + BLE scan).
4. On the sensor, **press and hold** the button ~3 seconds until the LED blinks fast.
5. Sensor appears in the hub list (`THS-temp_hum-XXXX` or `THS-contact-XXXX`).
6. **Select** → enter **name** and **location/room** → **Pair device**.
7. Hub writes Thread dataset + MQTT settings over BLE; sensor stops BLE, joins Thread, connects MQTT.
8. Device shows under **Paired devices**. Node-RED receives registry + state.

## Multi-pair

Window stays open after each successful pair so you can add several sensors. Click **Close pairing** when done.

## Factory reset sensor

Hold the button for **10 seconds at boot** (power-cycle while holding). NVS cleared; LED slow-blink = unpaired.

## Remove from hub

Dashboard → **Remove** on the device row (registry only; also factory-reset the sensor).

## LED meanings (sensors)

| LED | Meaning |
|-----|---------|
| Slow blink | Unpaired / idle |
| Fast blink | Pairing mode (advertising) |
| Solid | Provisioned and running |

## Ideas for later

- Identify: hub blinks sensor LED before confirm
- QR on sensor label encoding public type + serial
- Pair PIN for multi-tenant homes
- mDNS `thread-hub.local`
- OTA of sensor firmware from hub
- Import Matter commercial devices via separate Linux Matter bridge → same MQTT bus
