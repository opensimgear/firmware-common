# Native input producer for Cockpit reactive acquisition

This implements the portable producer portion of opensimgear/cockpit#191 and
#188. The device integration and host consumer cutover are separate work; this
library delivery does not claim an end-to-end preview latency improvement.

`osg_event_stream_start/prepare/frame_sent/stop` formats bounded unsolicited
`@events` lines from the existing physical event collector. The first frame is
a current baseline with no historical replay. Updates retain ordered edge
cursors, per-edge module capture timestamps and calibrated axis availability.
More pages drain without a host state-read timer. Quiet health records preserve
the collector's actual sample time. Pending frames remain immutable through
backpressure. Overflow and stream replacement produce explicit loss plus a
current baseline; counter/source exhaustion cannot manufacture healthy updates.

Local verification uses official LLVM/MinGW 20261020, asset SHA-256
`fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd`.
All eight native CTest suites pass. Portable core line coverage is
1,089/1,092 (99.73%); `events.c` is 152/154 (98.70%) and `stream.c` is
68/68 (100%). Tests include the existing release-to-rearm behavior, ordered
encoder pairs, capture times, paging, axis watchdog changes, immutable partial
delivery, overflow, scope replacement, exhaustion and small-buffer failure.

`osg_event_stream_test --vectors` emits nine actual wire frames for cross-language
verification. Cockpit's strict Rust decoder accepts the generated sequence and
checks paging, edge times, loss without replay and recovery. The direct USB
device's 19 native suites pass against this local library, including its partial
CDC-write integration and 100% line coverage of that delivery module.

These are software fixtures on Windows. Physical acquisition/USB timing,
firmware flashing and simulator effects remain unqualified in Cockpit follow-up
#199. No hardware was flashed. Final integration and CI remain with #188; CI is
deferred until feature implementation is complete, per the user's instruction.
