cmake_minimum_required(VERSION 3.17)

set(ViewerSourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ViewerIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ViewerLibDir ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(ViewerResourceDir ${CMAKE_CURRENT_SOURCE_DIR}/resources)

file(GLOB ViewerSources ${ViewerSourceDir}/*.cpp)
file(GLOB ViewerHeaders ${ViewerIncludeDir}/*.h)
file(GLOB ViewerResources ${ViewerResourceDir}/*)

add_executable(Viewer WIN32 ${ViewerSources} ${ViewerHeaders} ${ViewerResources})
target_include_directories(Viewer PUBLIC ${ViewerIncludeDir} ${ViewerIncludeDir}/private)

source_group("Resources" FILES ${ViewerResources})

