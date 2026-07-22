# Thread ESP32 Border Gateway

DIY Thread sensor hub with Hue-style pairing, OpenThread Border Router, MQTT, and a live network map dashboard.

## Architecture

    ESP32-H2 sensors (OpenThread + MQTT)
            | IEEE 802.15.4 Thread
            v
    ESP32-H2 RCP (Spinel UART)
            |
    ESP32-S3 hub (OTBR + Wi-Fi + BLE pair + web UI + MQTT)
            | Wi-Fi LAN
            v
    Mosquitto / Node-RED / Influx / Grafana

## Projects

| Folder | Chip | Role |
|--------|------|------|
| Thread_RCP | ESP32-H2 | Radio Co-Processor for the BR board |
| Thread_BorderGateway | ESP32-S3 | Hub: captive portal, OTBR, BLE pair, dashboard, MQTT |
| Thread_TempHum | ESP32-H2 | Temp/humidity end device (SHT30) |
| Thread_Contact | ESP32-H2 | Contact end device (scaffold) |

## Flash order (important)

1. RCP first — open Thread_RCP, upload to the ESP32-H2 on the Thread BR board.
2. Hub — open Thread_BorderGateway, upload to the ESP32-S3 USB port.
3. Sensor — open Thread_TempHum, upload to your SHT30 ESP32-H2 module.

### Spinel UART pins (hub config)

Default (IDF ot_br style), editable in Thread_BorderGateway/include/hub_config.h:

- RCP UART1 baud 460800
- Host RX GPIO4, TX GPIO5

On the Espressif integrated Thread BR board the H2 is wired onboard; if OTBR cannot talk to the RCP, adjust HUB_RCP_UART_RX_GPIO / TX_GPIO to your schematic.

## Hub setup

1. First boot: SoftAP ThreadHub-Setup-XXXX, portal http://192.168.4.1/
2. Enter Wi-Fi + MQTT, save, reboot onto LAN.
3. Dashboard: http://HUB_IP/ — devices, live readings, Mermaid network map.
4. Factory reset: Settings page, or hold BOOT 5s (8s at power-on).

## Pair a Temp/Humidity sensor (SHT30)

Wiring used by firmware:

- SDA GPIO5, SCL GPIO4, addr 0x44

1. Hub dashboard -> Add device
2. On sensor: hold button about 3s (fast LED) for pair mode
3. Select device, set name/room, Pair
4. Sensor joins Thread, publishes MQTT; map and table update live

Buttons on sensor:

- Hold about 3s: enter BLE pairing
- Single click: identify (sensor LED blink + hub status LED + MQTT)
- Hold 10s at boot: factory reset

## Identify

- Dashboard Identify on a device: hub LED + MQTT cmd to sensor
- Sensor click: local LED + home/broadcast/identify so hub LED blinks

## MQTT

See shared/protocol/mqtt_schema.md. Hub also subscribes to device state for the live dashboard.

## Build notes

- Hub and sensors use PlatformIO + ESP-IDF.
- ESP32-H2 needs a RISC-V toolchain (custom board JSON under boards/).
- PlatformIO packages may need esp32h2 treated like esp32c3/c6 for RISC-V toolchain selection.

## License

Private / project use unless otherwise stated.
