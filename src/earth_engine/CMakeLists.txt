# ---------------------------------------------------------------------------
# earth_engine_core —— 从 0 重建的引擎核心（命名空间 earth_engine）。
#
# 目录与 gis-md scaffold/src/earth_engine 同构（core/math、core/geodesy、
# core/async、core/net、…），保证阶段 6 并入 gis-md 现成地形服务时
# 命名空间 / 布局摩擦最小。
#
# 新增源文件请显式登记进对应模块列表（不 glob），构建确定性优先。
# 本阶段（阶段 0-1）：core/math + core/geodesy。
# ---------------------------------------------------------------------------
add_library(earth_engine_core STATIC)

# 公共包含根 = src/（这样任何地方 #include <earth_engine/core/math/Vec3.h>）
target_include_directories(earth_engine_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

set(MAP_CORE_MATH_SOURCES
    core/math/MathUtils.h
    core/math/Vec3.h
    core/math/Mat4.h
    core/math/Mat4.cpp
    core/math/Ray.h
    core/math/Rectangle.h
)

set(MAP_CORE_GEODESY_SOURCES
    core/geodesy/Cartographic.h
    core/geodesy/Ellipsoid.h
    core/geodesy/Ellipsoid.cpp
    core/geodesy/Transforms.h
    core/geodesy/Transforms.cpp
)

target_sources(earth_engine_core PRIVATE
    ${MAP_CORE_MATH_SOURCES}
    ${MAP_CORE_GEODESY_SOURCES}
)
