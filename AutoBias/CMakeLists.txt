cmake_minimum_required(VERSION 3.17)

set(BiasSourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(BiasIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB BiasSources ${BiasSourceDir}/*.cpp)
file(GLOB BiasHeaders ${BiasIncludeDir}/*.h)
file(GLOB BiasGeneratorHeaders ${BiasIncludeDir}/generator/*.h)

add_library(libAutoBias STATIC ${BiasSources} ${BiasHeaders} ${BiasGeneratorHeaders})

target_include_directories(libAutoBias PUBLIC ${BiasIncludeDir})
target_include_directories(libAutoBias PRIVATE ${GlobalIncludeDir})

target_link_directories(libAutoBias PUBLIC ${GlobalLibDir})
target_link_libraries(libAutoBias PUBLIC ArduinoControl)
