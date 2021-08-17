cmake_minimum_required(VERSION 3.17)

project(AutoBias VERSION 1.0
		 DESCRIPTION "AutoBias"
		 LANGUAGES CXX)

set(GlobalIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(GlobalLibDir ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_subdirectory(AutoBiasDevice)

add_subdirectory(ArduinoLED)
target_link_libraries(libArduinoControl PUBLIC libDevice)

add_subdirectory(TestDevice)
target_link_libraries(libTestDevice PUBLIC libDevice)

add_subdirectory(d3d12render)

add_subdirectory(DesktopCapture)

add_subdirectory(AutoBias)
target_link_libraries(libAutoBias PUBLIC libRender libArduinoControl libTestDevice)

add_subdirectory(QtTrayApp)

add_subdirectory(D3D12Viewer)
target_link_libraries(Viewer PUBLIC libRender libAutoBias libDesktopCapture libDevice libArduinoControl)
