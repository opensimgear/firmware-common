# Observable behavior tests

Build/CTest instructions are in the root README. Core tests exercise contact bounce, held-at-start recovery, new press/release, reconnect and millisecond wraparound; the bounded line parser is tested with normal, oversized and invalid input plus subsequent recovery. Input tests also exercise maintained positions, eight-way hat decoding and conflicts, axis scaling, encoder detent ordering/bounce, stalled report acknowledgements, pulse timing, bounded overflow and recovery. Generic-profile tests verify count/ownership/range limits, axis-only/button-only/hat-only descriptor field omission, shifter conflict/release rearming, cross-bank hat/selector failure, stale axis fallback, acknowledged encoder pulses and queued-event discard. Expansion-driver tests verify all register operations, missing/readback-fault recovery, eight-bank round-robin scanning, independent bank freshness, scan-stop expiry and tick wraparound. Optional USB tests inspect generated descriptor bytes, identities, interface/endpoint uniqueness and buffer failure behavior. Release builds keep assertions enabled.

USB topology tests also verify that zero-controller CDC-only configurations have no HID interface or endpoint, and that adding HID retains the same CDC descriptors and assigns HID interface 2.

`usb_topology_behavior` runs the pinned TinyUSB device/CDC/HID stack against a native controller stub. It submits bus resets and SET_CONFIGURATION requests for CDC-only → HID + CDC → CDC-only → HID + CDC, verifying CDC readiness, HID endpoint binding/reset, neutral absence of a HID driver instance and successful input-report completion when HID is present.

These host tests do not emulate the electrical USB bus, Windows enumeration or game bindings, and do not measure physical hardware. Device and cable/board validation remains separate.


`device_action_behavior` tests the shared advertised-action catalogue independently
of board callbacks: schema/revision, bounded pages, typed metadata, duplicate IDs,
invalid catalogue bounds, signed integer edges, booleans, RGB hex, enum options,
UTF-8 string hex/escaping, no-parameter one-shots, readable state and callback
failures. Invalid arguments, stale catalogue revisions, unknown actions and small
reply buffers must not invoke the device. The handler does not own transport
request deduplication; routed integration tests cover that separate boundary.

Configure with `OSG_TEST_COVERAGE=ON`, run CTest, then build the `coverage` target.
The portable core's weighted executable-line coverage must be strictly above
70%. This includes the action dispatcher and existing acquisition/profile/line
code; optional TinyUSB sources and electrical behavior are outside this measure.
GNU gcov is used on Linux, and `xcrun llvm-cov gcov` for Clang on macOS.
For a nondefault compiler, set `OSG_GCOV_COMMAND` to its matching gcov tool
(for example `gcov-15` with `gcc-15`). Reports remain in
`.research/native/coverage` (or the chosen scratch build directory). CI enables
this gate. Use a clean build after editing instrumented sources if retained
profiles no longer match the new objects.
