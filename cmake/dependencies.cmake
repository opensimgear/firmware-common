include(FetchContent)
# These pins are owned here; device projects pin firmware-common only.
function(osg_fetch name url revision)
  FetchContent_Declare(${name} GIT_REPOSITORY ${url} GIT_TAG ${revision}
    GIT_SUBMODULES "" SOURCE_SUBDIR _no_cmake)
  FetchContent_MakeAvailable(${name})
  set(${name}_SOURCE_DIR ${${name}_SOURCE_DIR} PARENT_SCOPE)
endfunction()
osg_fetch(tinyusb https://github.com/hathach/tinyusb.git efab607b8b738224c27b5e9e6410f8c74da43373)
if(OSG_EMBEDDED)
osg_fetch(stm32_hal https://github.com/STMicroelectronics/stm32g0xx-hal-driver.git 289ef6c8ff0922878e99d253f29be76851f8f28e)
osg_fetch(stm32_cmsis https://github.com/STMicroelectronics/cmsis-device-g0.git 7c77b7cbcee497dfbb5cc08f9562a80515e78013)
osg_fetch(cmsis_core https://github.com/ARM-software/CMSIS_5.git 55b19837f5703e418ca37894d5745b1dc05e4c91)
osg_fetch(libcanard https://github.com/OpenCyphal/libcanard.git 120600375535b6a383fbe99a9d77c04b7a65e81a)
# Protocol source is pinned for future ecosystem integration; hello-world CDC is diagnostic-only.
osg_fetch(osg_protocol https://github.com/opensimgear/protocol.git 5f72de7e855af67433e0dfcbd8859a7ab606dc6c)
endif()
