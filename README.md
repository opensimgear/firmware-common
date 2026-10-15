# OpenSimGear firmware common

Reusable C support for device firmware. Shared application protocol definitions remain in [protocol](https://github.com/opensimgear/protocol); physical pins, declarative device profiles and USB product identity stay in each device repository.

## Implemented starter support

- `osg_core`: debounced momentary buttons with release-to-rearm, maintained contacts, hat decoding, axis scaling, bounded quadrature encoder events, a bounded LF/CRLF diagnostic line parser, validated generic profiles, HID report descriptor/packing and portable TCA9555 input expansion, a separate bounded mapped-button/encoder transition log, plus device-advertised action descriptors, typed validation and command dispatch.
- `osg_usb`: CDC-only and HID + CDC configuration and composite-device descriptor helpers. Products retain their own HID reports and USB identity.
- `osg_tinyusb`: STM32G0 USB device stack.
- `osg_canard`: pinned libcanard library, including its bundled cavl2 dependency, available for transport integration. Current devices link the same pinned implementation through the self-contained `osg_cyphal` snapshot from protocol.
- Central full-commit pins for TinyUSB, STM32G0 HAL/CMSIS, CMSIS Core, libcanard and protocol.

The [device template](https://github.com/opensimgear/device-template) supplies the Rev B STM32G0B1CBT6 generic device and build-time input profiles. [Device gateway](https://github.com/opensimgear/device-gateway) supplies the STM32G0B1CBT6 USB-to-CAN gateway. Both use the shared Cyphal/CAN implementation and generated application DSDL from [protocol](https://github.com/opensimgear/protocol), retaining their identity, ownership, lease and USB delivery rules. This library supplies shared acquisition, USB, panel input events and temporary device-action support.

Physical STM32 hardware has not been validated. The device template implements portable runtime module settings/calibration using this library's input and profile helpers. Implementation and qualification work is tracked in the [GitHub project](https://github.com/orgs/opensimgear/projects/2).

## Native tests

CMake 3.24+, a native C compiler and Git are required. USB tests fetch pinned TinyUSB headers and device-stack sources; core-only tests need no external dependencies.

```sh
cmake -S . -B .research/native -DOSG_TEST_USB_DESCRIPTORS=ON -DOSG_TEST_COVERAGE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .research/native
ctest --test-dir .research/native --output-on-failure
cmake --build .research/native --target coverage
```

Assertions remain enabled in Release builds. Tests exercise debouncing, held-contact recovery, time wraparound, bounded line/overflow recovery, maintained-position debounce, hat conflicts, axis scaling, bounded encoder ordering/overflow/host stalls/recovery, and exported USB descriptors (composite interfaces, endpoints, length, caller-supplied identity, report length and a 64-byte HID endpoint). A native controller stub drives the pinned TinyUSB stack through CDC-only, HID + CDC and CDC-only configurations, checking driver binding/reset and HID readiness/report completion. It does not model the electrical USB bus or Windows controller identity. Generic-profile tests cover ownership/count/range rejection, exact field omission, 8 axes/128 logical buttons/4 hats bounds, startup and conflict release-to-rearm, cross-bank faults, axis freshness, encoder acknowledgements and eight-bank expansion polling/freshness/recovery. Device-action tests exercise catalogue validation/pagination, enum and signed-integer bounds, RGB, UTF-8/hex string encoding, no-parameter actions, readback, callback failures and rejected invocations without side effects. Omit `OSG_TEST_USB_DESCRIPTORS` for core-only tests. `OSG_TEST_COVERAGE` instruments native GNU/Clang builds; the `coverage` target requires weighted portable-core line coverage strictly above 70%, and CI enforces it. Run CTest before that target. Use a clean build directory after source edits when old coverage profiles have incompatible checksums.

## Device-advertised actions

`opensimgear/actions.h` supplies a bounded immutable action catalogue, callbacks and the shared `action describe <offset>`, `action state`, `action invoke <catalogueRevision> <id> <arguments...>` handler. Devices own stable IDs, labels, endpoint IDs and runtime values. Parameters support boolean, signed bounded integer, symbolic enum, RGB and bounded UTF-8 string values; actions can also have no parameters. Descriptors identify idempotent setters or one-shot operations and whether current software state is readable. The [wire contract](https://github.com/opensimgear/protocol/blob/main/serial/device-actions.md) defines JSON metadata, typed values, framing and routing.

Call `osg_actions_init()` with immutable descriptor storage, a nonzero catalogue revision, an invoke callback and a read callback for readable actions. It rejects duplicate/invalid identifiers, invalid UTF-8, unsupported types, inconsistent bounds and catalogue representations that exceed transport budgets. The product callback applies hardware behavior; this library owns command/type/range validation and bounded JSON serialization. `osg_actions_command()` accepts a full singular `action ...` line and returns false for unrelated commands without writing. Recognized failures return `ok:false` plus an error token. All accepted invocations require a 4096-byte reply buffer before executing; JSON reserves framing room and is at most 4091 bytes.

Devices call the same dispatcher from direct CDC and the routed CAN command handler. Actions require no configuration writer claim, do not write flash or change input/HID ownership, and can be used for online HID-disabled or direct USB devices. Temporary output values reset according to the device's reboot policy. The transport owns identity/boot/lease/epoch checks and duplicate request retention; this library does not infer delivery or retry one-shot operations. Invoke success confirms software application, not an electrical measurement. The current generic board advertises its existing status LED's auto/on/off modes; its native emulator advertises RGB and boolean LED endpoints.

A readable one-string setter may point `descriptor.display` at immutable
`osg_action_display_t` metadata: 1–63 columns, 1–4 rows, at most 63 total logical
cells, and ASCII or UTF-8 encoding. `max_bytes` remains the endpoint's independent
1–63 byte budget and must accommodate its cells. The handler rejects unsupported
ASCII characters and excess scalar cells before invoking, and checks readback too.
UTF-8 uses one Unicode scalar per logical cell, rather than terminal glyph width;
font/repertoire and physical display updates belong to the device callback.
One invocation assigns the complete row-major surface. Descriptors without
display metadata retain the original bounded-string behavior. Geometry/encoding
are additive schema-1 fields and a changed device catalogue requires a new revision.

`osg_actions_enable_ownership()` opts a validated registry into exclusive endpoint
leases and advertises its capability. Products dispatch with
`osg_actions_command_at()` using the current module clock and USB/CAN scope, tick
`osg_actions_tick()` independently of host traffic, and call
`osg_actions_invalidate_scope()` on affected transport loss. Secret nonzero UUID
tokens, endpoint alias exclusion, monotonically increasing lease IDs and
250–5000 ms TTLs prevent competing writers. Claim stores a fully validated
readable-setter fallback without applying it. Expiry or release attempts fallback
once and requires exact software readback; failures remain visible and never
retry an old fallback over a new owner. Legacy invokes and diagnostics respect
claims. The [wire contract](https://github.com/opensimgear/protocol/blob/main/serial/output-ownership.md)
defines claim/renew/invoke_owned/release and bounded paged public ownership.
Tokens never appear in replies. Ownership is volatile and separate from settings
claims. The command envelope is still 384 bytes and JSON at most 4091 bytes;
enablement rejects catalogues whose owned commands cannot fit. Legacy products
that do not enable ownership retain their original command behavior.

## Panel input events

`opensimgear/events.h` collects mapped button transitions and encoder detents in a
64-record volatile ring with 16-record nonconsuming pages. It owns separate
always-running acquisition, independent of product HID state and delivery ACKs.
`events state` provides a held-state/head baseline; `events read <stream> <after>`
validates stream/cursor and reports gaps, overflow and ordered edges. Products
reinitialize after input mapping activation and scope the stream to module boot.
The [panel event contract](https://github.com/opensimgear/protocol/blob/main/serial/panel-input-events.md)
defines baseline/reconnect behavior, control indexes and encoder press/release
pairs. Event reads use no configuration claim or persistent storage.

Call `osg_events_init()` with a validated immutable active profile, an initial
sample and its nonzero stream generation. Observe each fresh sample through
`osg_events_observe()` even when HID is inactive. The profile storage must outlive
the collector; sample pointers are not retained. `osg_events_command()` accepts
the full `events ...` command and requires a 4096-byte reply buffer. Reinitialize
after configuration activation, retaining the collector across HID ownership
changes. Event sequences never wrap: exhaustion fails closed until activation
or reboot. Native tests cover startup/recovery, short press/release retention,
independent encoder decoding, paging, immutable reads, overflow/gaps and bounds.

Event replies also advertise `contactButtonCount`, maintained logical indexes,
selector triples and axes in profile order. Axis snapshots include normalized
u16 values, usages, neutral values, per-axis availability and `sampledAtMs`.
They are current state rather than ring records; changing an axis does not consume
button sequence space. Fresh ADC samples expire after 100 ms and then return the
declared neutral with availability false. Register `osg_events_set_transform()`
after each init to apply the product runtime's inversion/deadzone to the separate
active acquisition report before capture. Its callback must transform only axes
and must not use or acknowledge product HID state. Initialization clears that
callback, cached samples and history, so activation must register it again.

Native tests include display encoding/cell/byte rejection and callback readback,
calibrated independent snapshots, sensor expiry, timestamp wrap and maximum-sized
control metadata/pages. They do not qualify physical displays or analog wiring.

## Embedded integration

A device pins this repository through FetchContent, sets `OSG_EMBEDDED=ON` and `OSG_USB_CONFIG_DIR` to its directory containing `tusb_config.h`, then links selected libraries. The STM32G0 USB target defines `STM32G0B1xx` and is used by both current module and gateway targets. Other MCU families require an explicit port. Header-only CMSIS and HAL source directory variables are exported to the device's parent build so its board can select HAL modules.

`osg_usb_device_descriptor(vid, pid, revision)` retains caller-supplied identity. `osg_usb_cdc_configuration()` exposes only CDC control/data interfaces 0/1. Gateways use this topology when there are no enabled controllers, so Windows sees the Cockpit console without a placeholder controller. `osg_usb_hid_cdc_configuration()` adds HID interface 2 with a 64-byte input endpoint; its CDC function is identical to the CDC-only topology. Switching configurations retains the gateway's VID, PID, serial string index, CDC interface numbers and endpoint addresses. Both helpers return the bytes written, or zero without writing for a null buffer or insufficient capacity. Standalone devices select HID + CDC when their active profile has inputs and CDC only for a zero-input profile. Products needing another topology should add a separate helper rather than change every device implicitly.

`opensimgear/inputs.h` supplies maintained-contact debounce (10 ms), eight-way hat mapping (null state 8), clamped unsigned axis scaling and a quadrature encoder queue. The encoder expects one four-transition cycle per detent anchored at both contacts open (`AB=3`). It queues at most 16 waiting events, diagnoses overflow/illegal jumps and waits for actual report completion before starting its 25 ms press/release intervals. Call `osg_encoder_reported()` only after the corresponding report reaches the host, not when it is merely submitted. Rearm on transport recovery to discard old queued events; inspect the counters for diagnostics. Device declarations own pin assignments, axis usages/ranges/neutral values and momentary/maintained/conflict policy; the shared profile engine constructs and processes those declarations consistently.

`opensimgear/profile.h` validates contact indexes (24 direct plus eight 16-contact banks), ADC indexes, report capacity and non-overlapping ownership. Encoders are restricted to direct inputs. Disabled controls are omitted from arrays and their descriptors. Report order is declared buttons, encoder pairs, selector triples, byte padding, packed hat nibbles/padding and 16-bit axes. Logical buttons total at most 128 including encoder/selector outputs; at most 8 axes, 4 hats, 8 encoders and 8 selectors are allowed. The largest valid input report is 34 bytes. A named zero-input profile is valid with report length zero; its descriptor function returns zero and products must select CDC-only USB, rather than add a placeholder controller. Momentary controls wait for release after startup/recovery; hats and exclusive button groups suppress conflicts. Failed expansion banks suppress only affected controls and fresh recovery re-arms safely. Axis freshness expires after 100 ms. `osg_profile_reported()` must receive only a completed report from the current transport generation.

`opensimgear/tca9555.h` uses caller-supplied seven-bit-address register callbacks. Each bank configures inputs and non-inverted polarity, verifies both settings and reads both ports. Failed transactions clear configuration and return no contacts. A round-robin poll processes at most one enabled address per tick. Input snapshots expire per bank after 100 ms, with wrap-safe ages, and never reuse stale contacts after failure. The device port owns bounded transaction deadlines and hardware bus recovery. TCA9555 contact pull-ups, address straps and bus loading are hardware responsibilities; fast encoders are not supported through this polled interface.

`osg_button_rearm()` blocks presses until a stable release; call it at startup and transport recovery. `osg_button_update()` accepts monotonic millisecond ticks with unsigned wraparound and returns current debounced pressed state. `osg_line_init()` requires a valid caller-owned buffer when capacity is nonzero; READY text remains valid until the next accepted byte. Overflow/NUL input is rejected through the next newline. CR is ignored for the diagnostic console. These are not production protocol framing APIs.

Keep reusable input and USB code here rather than copying it into device repositories or maintaining competing dependency pins. Each device's `shared-cyphal` snapshot is checked against the canonical protocol implementation; the generic device's `shared-gateway` snapshot retains only the flash journal. Use `FETCHCONTENT_SOURCE_DIR_FIRMWARE_COMMON` on a device build to test a candidate checkout, then update its full commit pin after review. Build changes against known device targets before release.

## Licensing

Copyright © Ivan Gregurić Ortolan. All rights reserved for original OpenSimGear material while licensing is under review. The [temporary licensing policy](LICENSE) defines the current scope. Creative external contributions are suspended during review; [CONTRIBUTING.md](CONTRIBUTING.md) explains the policy for factual bug reports and questions. Fetched third-party sources retain their upstream licenses and notices, collected in [THIRD_PARTY.md](THIRD_PARTY.md). Binary distributions require the applicable upstream notices.
