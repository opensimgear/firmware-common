# Platform ports

Board-specific ports live with their devices: `device-template/firmware/boards/starter_g0b1cb_v0_1_0` and `device-gateway/firmware/boards/gateway_g0b1cb_v0_1_0` both target STM32G0B1CBT6. Shared CMake builds TinyUSB's STM32G0 USB port and exports pinned HAL/CMSIS sources; this directory currently contains no additional port implementation. Both targets cross-build, while physical USB/CAN acceptance remains pending.
