cmake_minimum_required(VERSION 3.20)
project(map_cplus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(CTest)

# ---------------------------------------------------------------------------
# 核心库：src/earth_engine —— 命名空间 earth_engine，与 gis-md scaffold 布局
# 同构（core/math、core/geodesy、…），便于阶段 6 并入 gis-md 现成地形服务。
# ---------------------------------------------------------------------------
add_subdirectory(src/earth_engine)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
