# Platform ports

## Development status and disclaimer

This project is heavily under development and is not ready for use. Use it entirely at your own risk. To the fullest extent permitted by applicable law, the authors and contributors disclaim all liability for any damage, loss, or other issues arising from its use.

Board-specific ports live with their devices: `device-template/firmware/boards/starter_g0b1cb_v0_1_0` and `device-gateway/firmware/boards/gateway_g0b1cb_v0_1_0` both target STM32G0B1CBT6. Shared CMake builds TinyUSB's STM32G0 USB port and exports pinned HAL/CMSIS sources; this directory currently contains no additional port implementation. Both targets cross-build, while physical USB/CAN acceptance remains pending.
