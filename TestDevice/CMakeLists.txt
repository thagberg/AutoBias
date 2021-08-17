cmake_minimum_required(VERSION 3.17)

set(TestSourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TestIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB TestSources ${TestSourceDir}/*.cpp)
file(GLOB TestHeaders ${TestIncludeDir}/*.h)

add_library(libTestDevice STATIC ${TestSources} ${TestHeaders})
set_propertY(TARGET libTestDevice PROPERTY CXX_STANDARD 20)

target_include_directories(libTestDevice PUBLIC ${TestIncludeDir})
