cmake_minimum_required(VERSION 3.17)

set(ViewerSourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ViewerIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ViewerLibDir ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(ViewerResourceDir ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(ViewerShaderDir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

file(GLOB ViewerSources ${ViewerSourceDir}/*.cpp)
file(GLOB ViewerHeaders ${ViewerIncludeDir}/*.h)
file(GLOB ViewerResources ${ViewerResourceDir}/*)
file(GLOB ViewerShaders ${ViewerShaderDir}/*.hlsl)

add_executable(Viewer WIN32 ${ViewerSources} ${ViewerHeaders} ${ViewerResources} ${ViewerShaders})
set_property(TARGET Viewer PROPERTY CXX_STANDARD 20)
set_property(TARGET Viewer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

target_include_directories(Viewer PUBLIC ${ViewerIncludeDir} ${ViewerIncludeDir}/private)

set_source_files_properties(${ViewerShaders} PROPERTIES VS_TOOL_OVERRIDE "None")

source_group("Resources" FILES ${ViewerResources})
source_group("Shaders" FILES ${ViewerShaders})

add_custom_target(deploy_shaders ALL
	COMMAND xcopy /E /I /Y \"${ViewerShaderDir}\" \"${CMAKE_BINARY_DIR}/shaders\")

add_dependencies(Viewer deploy_shaders)
