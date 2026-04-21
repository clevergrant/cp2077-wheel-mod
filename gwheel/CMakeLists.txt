set(GWHEEL_SOURCES
  src/dllmain.cpp
  src/plugin.cpp
  src/device_table.cpp
  src/wheel_device.cpp
  src/vehicle_hooks.cpp
  src/ffb.cpp
  src/config.cpp
  src/rtti.cpp
  src/logging.cpp
)

set(GWHEEL_HEADERS
  src/plugin.h
  src/logging.h
  src/device_table.h
  src/wheel_device.h
  src/vehicle_hooks.h
  src/ffb.h
  src/config.h
  src/rtti.h
  include/gwheel_abi.h
)

add_library(gwheel SHARED
  ${GWHEEL_SOURCES}
  ${GWHEEL_HEADERS}
)

target_include_directories(gwheel
  PRIVATE
    src
    include
)

target_link_libraries(gwheel
  PRIVATE
    RED4ext::SDK
    dinput8
    dxguid
)

set_target_properties(gwheel PROPERTIES
  OUTPUT_NAME "gwheel"
  PREFIX ""
  SUFFIX ".dll"
)
