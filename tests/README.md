# Observable behavior tests

Build/CTest instructions are in the root README. Core tests exercise contact bounce, held-at-start recovery, new press/release, reconnect and millisecond wraparound; the bounded line parser is tested with normal, oversized and invalid input plus subsequent recovery. Optional USB tests inspect generated descriptor bytes, identities, interface/endpoint uniqueness and buffer failure behavior. Release builds keep assertions enabled.

These host tests do not emulate USB enumeration or measure physical hardware. Device and cable/board validation remains separate.
