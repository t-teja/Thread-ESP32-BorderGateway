# Flashing guide

## Gateway (S3 only)

The hub packages RCP firmware into a SPIFFS partition (rcp_fw). On first boot the S3 uses RESET/BOOT/UART to program the onboard ESP32-H2. You do not flash the H2 USB port for normal use.

### When RCP source changes (developer PC only)

1. Build Thread_RCP (esp32h2)
2. Run: python Thread_BorderGateway/tools/prepare_rcp_image.py
3. Flash hub to S3

### Flash the hub

1. Open Thread_BorderGateway
2. Build and upload to ESP32-S3
3. Serial 115200

Expected first boot:
- Wi-Fi or captive portal
- log: First-time / forced RCP program from S3
- automatic reboot
- OTBR becomes leader

### Espressif BR board pins (default in hub_config.h)

| Signal | GPIO |
|--------|------|
| Host RX (from RCP TX) | 17 |
| Host TX (to RCP RX) | 18 |
| RCP RESET | 7 |
| RCP BOOT | 8 |
| Spinel baud runtime | 460800 |

## Sensor (TempHum)

Open Thread_TempHum, upload to sensor ESP32-H2. SHT30 SDA=5 SCL=4 addr 0x44.

## Factory reset hub

Hold BOOT 5s while running, or 8s at power-on, or Settings page.

## If RCP never auto-flashes

1. Confirm full upload (not app-only). Serial upload log must mention rcp_fw.bin at 0x2c0000.
2. After reboot look for early logs: rcp_auto: RCP auto-update start.
3. If you see SPIFFS mount failed or used=0, the RCP partition was empty — rebuild hub and upload again.
4. Expected SPIFFS path inside partition: /rcp_fw/rcp_0/rcp_image.
