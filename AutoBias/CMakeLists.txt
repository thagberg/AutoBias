cmake_minimum_required(VERSION 3.17)

set(BiasSourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(BiasIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(BiasShaderDir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

file(GLOB BiasSources ${BiasSourceDir}/*.cpp)
file(GLOB BiasHeaders ${BiasIncludeDir}/*.h)
file(GLOB BiasGeneratorHeaders ${BiasIncludeDir}/generator/*.h)
file(GLOB BiasShaders ${BiasShaderDir}/*.hlsl)

add_library(libAutoBias STATIC ${BiasSources} ${BiasHeaders} ${BiasGeneratorHeaders} ${BiasShaders})
set_property(TARGET libAutoBias PROPERTY CXX_STANDARD 20)

set_source_files_properties(${BiasShaders} PROPERTIES VS_TOOL_OVERRIDE "None")

target_include_directories(libAutoBias PUBLIC ${BiasIncludeDir})
target_include_directories(libAutoBias PRIVATE ${GlobalIncludeDir})

target_link_directories(libAutoBias PUBLIC ${GlobalLibDir})

source_group("Shaders" FILES ${BiasShaders})

add_custom_target(CopyBiasShaders ALL
	COMMAND xcopy /E /I /Y \"${BiasShaderDir}\" \"${CMAKE_BINARY_DIR}/shaders\")

add_dependencies(libAutoBias CopyBiasShaders)
