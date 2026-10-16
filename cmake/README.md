# Dependency ownership

## Development status and disclaimer

This project is heavily under development and is not ready for use. Use it entirely at your own risk. To the fullest extent permitted by applicable law, the authors and contributors disclaim all liability for any damage, loss, or other issues arising from its use.

`dependencies.cmake` owns exact upstream commit pins. Device projects pin firmware-common and select this support with `OSG_EMBEDDED`; they do not repeat TinyUSB/libcanard/HAL pins. FetchContent local source overrides support development and offline builds from prepared checkouts. Dependencies are fetched without their test submodules or upstream build scripts.

Pin changes require shared native tests plus a cross-build of known device targets. libcanard's bundled cavl2 header is part of its pinned source. Current device builds link protocol's hash-verified `shared-cyphal` snapshot with generated DSDL and the same libcanard revision. They do not link a second `osg_canard` copy into their firmware.
