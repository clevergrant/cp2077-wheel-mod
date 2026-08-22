set(LOGI_SDK_ROOT "${CMAKE_SOURCE_DIR}/vendor/LogitechSDK_unpacked")
set(LOGI_SDK_LIB "${LOGI_SDK_ROOT}/Lib/x64/LogitechSteeringWheelLib.lib")

if(EXISTS "${LOGI_SDK_LIB}")
  add_executable(wheel_reset wheel_reset.cpp)
  target_include_directories(wheel_reset PRIVATE "${LOGI_SDK_ROOT}/Include")
  target_link_libraries(wheel_reset PRIVATE "${LOGI_SDK_LIB}")
  set_target_properties(wheel_reset PROPERTIES OUTPUT_NAME "wheel_reset")
endif()
