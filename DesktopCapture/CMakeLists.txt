cmake_minimum_required(VERSION 3.17)

set(DCSourceDir "src")
set(DCIncludeDir "include")
set(DCLibDir "lib")

file(GLOB DCSources ${DCSourceDir}/*.cpp)
file(GLOB DCHeaders ${DCIncludeDir}/*.h)

add_library(libDesktopCapture STATIC ${DCSources} ${DCHeaders})
target_include_directories(libDesktopCapture PUBLIC ${DCIncludeDir})
