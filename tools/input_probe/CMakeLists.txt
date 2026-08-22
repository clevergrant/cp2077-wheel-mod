add_executable(input_probe input_probe.cpp)
target_link_libraries(input_probe PRIVATE dinput8 dxguid)
set_target_properties(input_probe PROPERTIES OUTPUT_NAME "input_probe")

