## Captive portal

On first boot the hub opens SoftAP ThreadHub-Setup-XXXX. Configure Wi-Fi and MQTT at http://192.168.4.1/ — secrets.h is not required.

# Flashing with VS Code + PlatformIO

## Prerequisites

- VS Code + PlatformIO IDE extension
- USB data cable
- ESP Thread BR board (ESP32-S3 host USB) for gateway
- ESP32-H2 dev board(s) for sensors
- Python 3 (PlatformIO bundles toolchains on first build)

## Border Gateway

1. Open folder `Thread_BorderGateway` in VS Code (or open the repo and select the PIO project).
2. Copy `include/secrets.h.example` → `include/secrets.h`.
3. Set `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_HOST` (mini PC LAN IP), optional MQTT auth.
4. Connect the **ESP32-S3** USB port of the BR board (not only the H2).
5. PlatformIO: **Build** then **Upload**.
6. **Monitor** at 115200. Note the IP address.
7. Browser: `http://<hub-ip>/`

### RCP (ESP32-H2 on BR board)

Full production BR builds embed/update RCP via Spinel auto-flash (esp-thread-br).  
This repo’s **phase-1** gateway runs the hub application (Wi-Fi, web UI, BLE pair, registry, MQTT) and includes OpenThread BR hooks that you enable once IDF OpenThread BR + RCP binaries are linked (see `docs/OTBR_IDF.md`).

Until OTBR is fully linked, you can still validate pairing BLE + web UI + MQTT registry end-to-end; Thread attach completes after OTBR is enabled.

## Temp / humidity sensor

1. Open `Thread_TempHum`.
2. Optional: set `BOARD_BUTTON_GPIO` / LED / I2C pins in `include/board.h`.
3. Select env `esp32h2`, connect the H2 board, Upload + Monitor.
4. Hold **BOOT/USER** button ~3 s → fast LED blink = pairing mode.
5. On hub UI → Add device → select sensor → name/room → Pair.

## Contact sensor

Same as TempHum under `Thread_Contact`. Wire reed switch to `CONTACT_GPIO` (see `board.h`).

## Useful PIO commands

```bash
cd Thread_BorderGateway
pio run -t upload
pio device monitor -b 115200

cd ../Thread_TempHum
pio run -e esp32h2 -t upload
```

## Troubleshooting

| Issue | Check |
|-------|--------|
| Port not found | Cable, driver, correct S3 vs H2 port |
| Wi-Fi fail | secrets.h, 2.4 GHz SSID |
| Sensor not listed | Hub pair window open; sensor in pair mode; near hub |
| MQTT offline | Broker IP reachable from hub Wi-Fi; port 1883 |
| Thread not attaching | OTBR enabled; same dataset; channel not blocked |
