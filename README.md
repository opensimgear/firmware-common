# OpenSimGear firmware common

Reusable C support for device firmware. Shared application protocol definitions remain in [protocol](https://github.com/opensimgear/protocol); physical pins, declarative device profiles and USB product identity stay in each device repository.

## Implemented starter support

- `osg_core`: debounced momentary buttons with release-to-rearm, maintained contacts, hat decoding, axis scaling, bounded quadrature encoder events, a bounded LF/CRLF diagnostic line parser, validated generic profiles, HID report descriptor/packing and portable TCA9555 input expansion.
- `osg_usb`: HID + CDC configuration and composite-device descriptor helpers. This is an opt-in starter layout, not a universal report imposed on products.
- `osg_tinyusb`: STM32G0 USB device stack.
- `osg_canard`: pinned libcanard library, including its bundled cavl2 dependency.
- Central full-commit pins for TinyUSB, STM32G0 HAL/CMSIS, CMSIS Core, libcanard and protocol.

The [device template](https://github.com/opensimgear/device-template) supplies the real STM32G0B1CBT6 board target. Compile/host tests do not establish hardware validation. Discovery, application Cyphal transfers, output ownership, persistence, production configuration and update behavior remain future work.

## Native tests

CMake 3.24+, a native C compiler and Git are required. USB descriptor tests fetch pinned upstream sources for headers; core-only tests need no external dependencies.

```sh
cmake -S . -B build/native -DOSG_TEST_USB_DESCRIPTORS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

Assertions remain enabled in Release builds. Tests exercise debouncing, held-contact recovery, time wraparound, bounded line/overflow recovery, maintained-position debounce, hat conflicts, axis scaling, bounded encoder ordering/overflow/host stalls/recovery, and exported USB descriptors (composite interfaces, endpoints, length, caller-supplied identity, report length and a 64-byte HID endpoint). Generic-profile tests cover ownership/count/range rejection, exact field omission, 8 axes/128 logical buttons/4 hats bounds, startup and conflict release-to-rearm, cross-bank faults, axis freshness, encoder acknowledgements and eight-bank expansion polling/freshness/recovery. Omit `OSG_TEST_USB_DESCRIPTORS` for core-only tests.

## Embedded integration

A device pins this repository through FetchContent, sets `OSG_EMBEDDED=ON` and `OSG_USB_CONFIG_DIR` to its directory containing `tusb_config.h`, then links selected libraries. The STM32G0 USB target defines `STM32G0B1xx`; this port is not automatically usable by other MCUs or the undecided gateway platform. Header-only CMSIS and HAL source directory variables are exported to the device's parent build so its board can select HAL modules.

`osg_usb_device_descriptor(vid, pid, revision)` retains caller-supplied identity. `osg_usb_hid_cdc_configuration()` returns the bytes written, or zero without writing if capacity is insufficient. It uses one CDC control/data pair and one HID interface with a 64-byte input endpoint; products needing another topology should add a separate helper rather than change every device implicitly.

`opensimgear/inputs.h` supplies maintained-contact debounce (10 ms), eight-way hat mapping (null state 8), clamped unsigned axis scaling and a quadrature encoder queue. The encoder expects one four-transition cycle per detent anchored at both contacts open (`AB=3`). It queues at most 16 waiting events, diagnoses overflow/illegal jumps and waits for actual report completion before starting its 25 ms press/release intervals. Call `osg_encoder_reported()` only after the corresponding report reaches the host, not when it is merely submitted. Rearm on transport recovery to discard old queued events; inspect the counters for diagnostics. Device declarations own pin assignments, axis usages/ranges/neutral values and momentary/maintained/conflict policy; the shared profile engine constructs and processes those declarations consistently.

`opensimgear/profile.h` validates contact indexes (24 direct plus eight 16-contact banks), ADC indexes, report capacity and non-overlapping ownership. Encoders are restricted to direct inputs. Disabled controls are omitted from arrays and their descriptors. Report order is declared buttons, encoder pairs, selector triples, byte padding, packed hat nibbles/padding and 16-bit axes. Logical buttons total at most 128 including encoder/selector outputs; at most 8 axes, 4 hats, 8 encoders and 8 selectors are allowed. The largest valid input report is 34 bytes. Momentary controls wait for release after startup/recovery; hats and exclusive button groups suppress conflicts. Failed expansion banks suppress only affected controls and fresh recovery re-arms safely. Axis freshness expires after 100 ms. `osg_profile_reported()` must receive only a completed report from the current transport generation.

`opensimgear/tca9555.h` uses caller-supplied seven-bit-address register callbacks. Each bank configures inputs and non-inverted polarity, verifies both settings and reads both ports. Failed transactions clear configuration and return no contacts. A round-robin poll processes at most one enabled address per tick. Input snapshots expire per bank after 100 ms, with wrap-safe ages, and never reuse stale contacts after failure. The device port owns bounded transaction deadlines and hardware bus recovery. TCA9555 contact pull-ups, address straps and bus loading are hardware responsibilities; fast encoders are not supported through this polled interface.

`osg_button_rearm()` blocks presses until a stable release; call it at startup and transport recovery. `osg_button_update()` accepts monotonic millisecond ticks with unsigned wraparound and returns current debounced pressed state. `osg_line_init()` requires a valid caller-owned buffer when capacity is nonzero; READY text remains valid until the next accepted byte. Overflow/NUL input is rejected through the next newline. CR is ignored for the diagnostic console. These are not production protocol framing APIs.

Do not copy shared sources into device repositories or maintain competing dependency pins. Use `FETCHCONTENT_SOURCE_DIR_FIRMWARE_COMMON` on a device build to test a candidate checkout, then update its full commit pin after review. Build changes against known device targets before release.

## Licensing

OpenSimGear license selection remains pending; no grant is implied for newly authored code. Fetched third-party sources retain their upstream notices, collected in [THIRD_PARTY.md](THIRD_PARTY.md). Include appropriate notices with any future binary distribution.
