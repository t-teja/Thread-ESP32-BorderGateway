# Enabling full OpenThread Border Router

Phase-1 hub firmware implements application services (Wi-Fi, HTTP, BLE provisioner, registry, MQTT). Full Thread BR uses Espressif’s **esp-thread-br** / IDF OpenThread BR on the ESP32-S3 with on-board ESP32-H2 as RCP.

## Recommended upstream base

- ESP-IDF **v5.5.x** (as required by current esp-thread-br docs)
- Repo: https://github.com/espressif/esp-thread-br
- Example: `examples/basic_thread_border_router`
- Hardware: ESP Thread BR / Zigbee GW board (S3 + H2)

## Integration steps (when ready)

1. Install matching ESP-IDF and clone `esp-thread-br` next to this repo (or as git submodule under `third_party/esp-thread-br`).
2. Build `ot_rcp` for ESP32-H2; BR example embeds RCP for first-boot flash over UART.
3. Merge this project’s components into the BR example **or** add BR components/config to `Thread_BorderGateway`:
   - `CONFIG_OPENTHREAD_BORDER_ROUTER=y`
   - `CONFIG_OPENTHREAD_RADIO_SPINEL_UART=y` (pins per BR board)
   - `CONFIG_OPENTHREAD_BR_AUTO_START` / NAT64 / SRP as needed
4. Implement `otbr_net.c` hooks:
   - form/attach network on first boot
   - export **active operational dataset** TLVs for BLE provisioning
   - optional: permit-join / joiner if you add MeshCoP later
5. Sensors already accept `dataset` (base64 TLVs) in the provision command.

## Dual workflow

| Goal | Tool |
|------|------|
| App UX, pairing, MQTT, registry | This PlatformIO project |
| Certified-style BR radio stack | esp-thread-br example |

Long-term: one firmware image = BR libs + this hub app.

## Dataset backup

After first form, save dataset hex/base64 from hub API `GET /api/thread/dataset` (when OTBR linked) or OT CLI `dataset active -x`. Store offline; all sensors depend on it.
