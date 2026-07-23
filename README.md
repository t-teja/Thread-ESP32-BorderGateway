# Thread ESP32 Border Gateway

DIY Thread sensor hub with Hue-style pairing, OpenThread Border Router, MQTT, and a live network map dashboard.

## Architecture

    ESP32-H2 sensors (OpenThread + CoAP to hub)
            | IEEE 802.15.4 Thread
            v
    ESP32-H2 RCP (Spinel UART)
            |
    ESP32-S3 hub (OTBR + Wi-Fi + BLE pair + web UI + MQTT bridge)
            | Wi-Fi LAN
            v
    Mosquitto / Node-RED / Influx / Grafana

Sensors never speak MQTT — they report to the hub over Thread (CoAP) and the hub is the
only MQTT client, bridging to the broker. See docs/ARCHITECTURE.md.

## Projects

| Folder | Chip | Role |
|--------|------|------|
| Thread_RCP | ESP32-H2 | Radio Co-Processor source — built and embedded into the hub image, **not flashed directly** (see below) |
| Thread_BorderGateway | ESP32-S3 | Hub: captive portal, OTBR, BLE pair, dashboard, MQTT |
| Thread_TempHum | ESP32-H2 | Temp/humidity end device (SHT30) |
| Thread_Contact | ESP32-H2 | Contact end device (scaffold) |

## Flash order (important)

You only flash two boards for a working system:

1. Hub — open Thread_BorderGateway, upload to the ESP32-S3 USB port. The hub embeds RCP firmware in a SPIFFS partition and auto-programs the onboard/attached ESP32-H2 over UART (RESET/BOOT GPIOs) on first boot, or whenever the packaged RCP image changes. See "Gateway flash (S3 only)" below.
2. Sensor — open Thread_TempHum (or Thread_Contact), upload to your ESP32-H2 sensor module.

`Thread_RCP` is not a separate flashing step. It is the source project used to (re)build the RCP firmware binary that gets packaged into the hub's image — you only touch it as a developer when the RCP firmware itself needs to change (see "Gateway flash (S3 only)").

### Spinel UART pins (hub config)

Default (Espressif Thread BR board wiring), editable in Thread_BorderGateway/include/hub_config.h:

- RCP UART1 baud 460800
- Host RX GPIO17 (from RCP TX), Host TX GPIO18 (to RCP RX)
- RCP RESET GPIO7, RCP BOOT GPIO8

If OTBR cannot talk to the RCP, adjust HUB_RCP_UART_RX_GPIO / TX_GPIO / RESET / BOOT to your schematic.

### Gateway flash (S3 only) — updating RCP firmware

Developer PC workflow, only needed when Thread_RCP source changes:

1. Build Thread_RCP: `pio run -e esp32h2` (in `Thread_RCP/`)
2. Repackage the RCP image for the hub: `python Thread_BorderGateway/tools/prepare_rcp_image.py`
3. Bump `HUB_RCP_PKG_VER` in `Thread_BorderGateway/src/rcp_auto.c` — this forces a one-time reflash of the H2 on next hub boot even if a device was already provisioned (NVS `pkg_ver` mismatch triggers `do_force_update()`).
4. Build + flash Thread_BorderGateway to the S3: `pio run -e esp32s3 -t upload`

Expected boot log on a forced update: `RCP auto-update start` -> `RCP_UPDATE: Progress: 0..100 %` -> `Finished programming` / `Flash verified` -> `RCP update OK — mark verified and reboot` -> hub reboots -> OTBR comes up (`OpenThread attached to netif`, then `leader`/`router`).

Hub status LED (ESP32-S3, GPIO48) blinks 3x slow on a successful RCP flash, 8x fast if the flash failed. RCP board LED (ESP32-H2, GPIO22, active-low) pulses briefly every ~2s as a heartbeat while the RCP firmware is running.

See docs/FLASHING.md.

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
4. Sensor joins Thread and reports to the hub via CoAP; hub bridges to MQTT — map and table update live

Buttons on sensor:

- Hold about 3s: enter BLE pairing
- Single click: identify (sensor LED blink + hub status LED via CoAP)
- Hold 10s at boot: factory reset

## Identify

- Dashboard Identify on a device: hub LED + CoAP cmd to sensor
- Sensor click: local LED + CoAP event to hub so hub LED blinks

## MQTT

See shared/protocol/mqtt_schema.md and docs/ARCHITECTURE.md. The hub is the only MQTT
client — sensors talk Thread-only (CoAP) to the hub, which bridges reports/commands
to/from the broker.

## Build notes

- Hub and sensors use PlatformIO + ESP-IDF.
- ESP32-H2 needs a RISC-V toolchain (custom board JSON under boards/).
- PlatformIO packages may need esp32h2 treated like esp32c3/c6 for RISC-V toolchain selection.

## License

Private / project use unless otherwise stated.
