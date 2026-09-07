cmake_minimum_required(VERSION 3.20)
project(mapcplus_terrain_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# core 库的第三方头（stb 等）：android 构建环境需 GIS_MD_VCPKG_INSTALLED
# （由构建脚本 export；缺省回落本机 host vcpkg 路径）。
if(NOT DEFINED MAP_THIRDPARTY_INCLUDE_DIR)
    if(DEFINED ENV{GIS_MD_VCPKG_INSTALLED})
        set(MAP_THIRDPARTY_INCLUDE_DIR "$ENV{GIS_MD_VCPKG_INSTALLED}/include")
    else()
        set(MAP_THIRDPARTY_INCLUDE_DIR
            "/Users/yan/Desktop/work/gis-md/scaffold/third_party/vcpkg/installed/arm64-osx/include")
    endif()
endif()

# 编入本仓 core（从 0 自写核心）
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../src/earth_engine
                 ${CMAKE_CURRENT_BINARY_DIR}/earth_engine_core)

add_library(terrain_demo SHARED demo_scene.cpp native_bridge.cpp dem_assets.cpp gles3_device.cpp)
target_compile_options(terrain_demo PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(terrain_demo PRIVATE earth_engine_core android log GLESv3)
