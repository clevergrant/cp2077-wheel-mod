set(GWHEEL_SOURCES
  src/dllmain.cpp
  src/plugin.cpp
  src/device_table.cpp
  src/wheel.cpp
  src/sources.cpp
  src/vehicle_hook.cpp
  src/input_bindings.cpp
  src/kbd_hook.cpp
  src/config.cpp
  src/rtti.cpp
  src/rtti_dump.cpp
  src/logging.cpp
)

set(GWHEEL_HEADERS
  src/plugin.h
  src/logging.h
  src/device_table.h
  src/wheel.h
  src/sources.h
  src/vehicle_hook.h
  src/input_bindings.h
  src/kbd_hook.h
  src/config.h
  src/rtti.h
  src/rtti_dump.h
  include/gwheel_abi.h
)

add_library(gwheel SHARED
  ${GWHEEL_SOURCES}
  ${GWHEEL_HEADERS}
)

# Logitech Steering Wheel SDK (vendored under gwheel/vendor/LogitechSDK_unpacked/).
set(LOGI_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/LogitechSDK_unpacked")
set(LOGI_SDK_INCLUDE_DIR "${LOGI_SDK_ROOT}/Include")
set(LOGI_SDK_LIB "${LOGI_SDK_ROOT}/Lib/x64/LogitechSteeringWheelLib.lib")
if(NOT EXISTS "${LOGI_SDK_LIB}")
  message(FATAL_ERROR
    "Logitech SDK import lib not found at ${LOGI_SDK_LIB}. "
    "Ensure gwheel/vendor/LogitechSDK_unpacked/ is present.")
endif()

target_include_directories(gwheel
  PRIVATE
    src
    include
    "${LOGI_SDK_INCLUDE_DIR}"
)

target_link_libraries(gwheel
  PRIVATE
    RED4ext::SDK
    "${LOGI_SDK_LIB}"
    dinput8
    dxguid
)

set_target_properties(gwheel PROPERTIES
  OUTPUT_NAME "gwheel"
  PREFIX ""
  SUFFIX ".dll"
)
