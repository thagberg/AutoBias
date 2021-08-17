cmake_minimum_required(VERSION 3.17)

set(DCSourceDir "src")
set(DCIncludeDir "include")
set(DCLibDir "lib")

file(GLOB DCSources ${DCSourceDir}/*.cpp)
file(GLOB DCHeaders ${DCIncludeDir}/*.h)

add_library(libDesktopCapture STATIC ${DCSources} ${DCHeaders})
set_property(TARGET libDesktopCapture PROPERTY CXX_STANDARD 20)
target_include_directories(libDesktopCapture PUBLIC ${DCIncludeDir})
