set(GWHEEL_SOURCES
  src/dllmain.cpp
  src/plugin.cpp
  src/device_table.cpp
  src/wheel.cpp
  src/dinput_manager.cpp
  src/dinput_backend.cpp
  src/moza_backend.cpp
  src/sources.cpp
  src/vehicle_hook.cpp
  src/input_bindings.cpp
  src/kbd_hook.cpp
  src/config.cpp
  src/rtti.cpp
  src/rtti_dump.cpp
  src/rtti_offsets.cpp
  src/logging.cpp
  src/audio_monitor.cpp
  src/led.cpp
  src/mod_settings_seed.cpp
)

set(GWHEEL_HEADERS
  src/plugin.h
  src/logging.h
  src/device_table.h
  src/wheel.h
  src/ffb_backend.h
  src/dinput_manager.h
  src/dinput_backend.h
  src/moza_backend.h
  src/sources.h
  src/vehicle_hook.h
  src/vehicle_physics_ext.h
  src/input_bindings.h
  src/kbd_hook.h
  src/config.h
  src/rtti.h
  src/rtti_dump.h
  src/rtti_offsets.h
  src/audio_monitor.h
  src/led.h
  src/mod_settings_seed.h
  include/gwheel_abi.h
)

add_library(gwheel SHARED
  ${GWHEEL_SOURCES}
  ${GWHEEL_HEADERS}
)

set(LOGI_SDK_ROOT "${CMAKE_SOURCE_DIR}/vendor/LogitechSDK_unpacked")
set(LOGI_SDK_INCLUDE_DIR "${LOGI_SDK_ROOT}/Include")
set(LOGI_SDK_LIB "${LOGI_SDK_ROOT}/Lib/x64/LogitechSteeringWheelLib.lib")

target_include_directories(gwheel
  PRIVATE
    src
    include
    "${LOGI_SDK_INCLUDE_DIR}"
)

target_link_libraries(gwheel
  PRIVATE
    RED4ext::SDK
    dinput8
    dxguid
    ole32
    uuid
    mmdevapi
)

if(EXISTS "${LOGI_SDK_LIB}")
  target_link_libraries(gwheel PRIVATE "${LOGI_SDK_LIB}")
endif()

set_target_properties(gwheel PROPERTIES
  OUTPUT_NAME "gwheel"
  PREFIX ""
  SUFFIX ".dll"
)
