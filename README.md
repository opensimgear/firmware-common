# OpenSimGear firmware common

Reusable C firmware implementation for OpenSimGear modules and gateway firmware. Shared protocol definitions live in the separate [protocol](https://github.com/opensimgear/protocol) repository.

## Status and ownership

Repository scaffold initialized; reusable implementations and validated build targets are not yet present.

- include/opensimgear/: public C interfaces.
- src/usb/: descriptor builders and HID/CDC behavior.
- src/can/: Cyphal transport, discovery, and common routing mechanisms.
- src/inputs/: debouncing, release-to-rearm behavior, encoder queues, and recovery handling.
- src/outputs/: ownership, freshness, and output failure handling.
- src/config/: version/compatibility checks, one-writer sessions, and interrupted-write protection.
- src/diagnostics/: health and diagnostic mechanisms.
- ports/: optional validated platform bindings.
- cmake/: shared build integration and dependency ownership.
- tests/: observable behavior and protocol conformance tests as implementation arrives.

Device repositories retain pin maps, board targets, capability/control profiles, USB product identity, and device-specific policies. This repository must not impose one descriptor or pinout on every device.

## Dependencies

The planned firmware baseline uses C, TinyUSB, and libcanard. Add exact tested protocol/TinyUSB/libcanard revisions in one central dependency configuration during integration. Device firmware pins this repository to a full commit and can explicitly override it with a local checkout for development. Do not copy shared sources or maintain competing dependency pins.

## Validation and releases

Test observable inputs, outputs, and side effects. Once device targets exist, validate candidate common-code changes against known device baselines by overriding their common dependency with the candidate revision. Release compatibility information without implying hardware validation from compilation alone.

## Related repositories

- [Protocol](https://github.com/opensimgear/protocol): shared definitions and compatibility rules.
- [Firmware common](https://github.com/opensimgear/firmware-common): reusable C implementation.
- [Device template](https://github.com/opensimgear/device-template): new-device scaffold with planned hello-world firmware and starter KiCad board.
- [Cockpit](https://github.com/opensimgear/cockpit): desktop configuration application.
- [Website](https://github.com/opensimgear/opensimgear.github.io): system architecture and user documentation.

## Licensing

License selection is pending. No license grant is implied by this scaffold. Firmware, mechanical design, and electronics will each need documented license coverage before releases and any public USB identifier application.
