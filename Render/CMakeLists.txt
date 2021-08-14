cmake_minimum_required(VERSION 3.17)

set(RenderSourceDir "src")
set(RenderPublicIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(RenderPrivateIncludeDir ${RenderPublicIncludeDir}/private)
set(RenderLibDir "lib")

file(GLOB RenderSources ${RenderSourceDir}/*.cpp)
file(GLOB RenderHeaders ${RenderPublicIncludeDir}/*.h)
add_library(libRender STATIC ${RenderSources} ${RenderHeaders})
target_include_directories(libRender PUBLIC ${RenderPublicIncludeDir})
target_include_directories(libRender PRIVATE ${RenderPrivateIncludeDir})

target_link_libraries(libRender PUBLIC d3d12)
target_link_libraries(libRender PUBLIC dxgi)
