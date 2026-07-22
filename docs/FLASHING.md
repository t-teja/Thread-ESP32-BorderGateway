# Flashing guide

## 1) RCP (ESP32-H2 on BR board)

    cd Thread_RCP
    pio run -e esp32h2 -t upload

Use the USB port connected to the H2.

## 2) Hub (ESP32-S3)

    cd Thread_BorderGateway
    pio run -e esp32s3 -t upload
    pio device monitor -b 115200

Expect Wi-Fi, then OTBR ready/leader after RCP is correct. If OTBR start timeout, re-check RCP flash and Spinel pins.

## 3) TempHum sensor

    cd Thread_TempHum
    pio run -e esp32h2 -t upload

SHT30: SDA=5 SCL=4 addr 0x44.

## Captive portal / factory reset

- SoftAP ThreadHub-Setup-XXXX -> http://192.168.4.1/
- Hold BOOT 5s while running, or 8s at power-on
