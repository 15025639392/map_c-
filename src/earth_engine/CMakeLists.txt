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
    core/math/Vec2.h
    core/math/Vec3.h
    core/math/Mat4.h
    core/math/Mat4.cpp
    core/math/Ray.h
    core/math/RayTriangle.h
    core/math/Rectangle.h
    core/math/Plane.h
)

set(MAP_CORE_GEODESY_SOURCES
    core/geodesy/Cartographic.h
    core/geodesy/Ellipsoid.h
    core/geodesy/Ellipsoid.cpp
    core/geodesy/RayEllipsoid.h
    core/geodesy/RayEllipsoid.cpp
    core/geodesy/Projection.h
    core/geodesy/Projection.cpp
    core/geodesy/QuadtreeGeometricError.h
    core/geodesy/QuadtreeGeometricError.cpp
    core/geodesy/HeightDatumCorrector.h
    core/geodesy/HeightDatumCorrector.cpp
    core/geodesy/Transforms.h
    core/geodesy/Transforms.cpp
)

# tiling：瓦片键/四叉树与瓦片 scheme（纯数学，无引擎依赖；阶段 3 起）。
# gis-md 同名模块承载瓦片树/LOD/调度——本仓从瓦片网格地基开始，逐轮扩充。
set(MAP_TILING_SOURCES
    tiling/TileKey.h
    tiling/WebMercatorTileScheme.h
    tiling/WebMercatorTileScheme.cpp
    tiling/TerrainLodSelector.h
    tiling/TerrainLodSelector.cpp
)

# content：地形内容（高度图编解码/采样）。gis-md 同名模块含 heightmap/glTF/
# 上采样/位移模板；本仓从编解码地基开始，与地形链路同步扩充。
set(MAP_CONTENT_SOURCES
    content/HeightmapCodec.h
    content/HeightmapCodec.cpp
    content/HeightmapSampler.h
    content/HeightmapSampler.cpp
    content/HeightmapTile.h
    content/HeightmapTile.cpp
    content/TerrainTileMesh.h
    content/TerrainTileMesh.cpp
    content/TerrainDataSource.h
    content/TerrainFrameAssembler.h
    content/TerrainFrameAssembler.cpp
    content/AncestorFallbackDataSource.h
    content/AncestorFallbackDataSource.cpp
    content/HeightDatumCorrectingDataSource.h
    content/HeightDatumCorrectingDataSource.cpp
    content/SeamAudit.h
    content/SeamAudit.cpp
    content/TerrainPicking.h
    content/TerrainPicking.cpp
    content/TerrainFrameCache.h
    content/TerrainFrameCache.cpp
)

# camera：地表相机视图地基（位姿基/射线/地表脚印/地形帧管线）。
set(MAP_CAMERA_SOURCES
    camera/CameraView.h
    camera/CameraView.cpp
    camera/TerrainCameraPipeline.h
    camera/TerrainCameraPipeline.cpp
    camera/Frustum.h
    camera/Frustum.cpp
)

# providers：瓦片数据源（URL 模板 + 字节源 + Terrain-RGB 高度源 + PNG 解码）。
set(MAP_PROVIDERS_SOURCES
    providers/TileUrlFormatter.h
    providers/TileUrlFormatter.cpp
    providers/ITileBytesSource.h
    providers/TerrainRgbTileSource.h
    providers/TerrainRgbTileSource.cpp
    providers/StbPngDecoder.h
    providers/StbPngDecoder.cpp
    providers/TerrainRgbPngTileSource.h
    providers/TerrainRgbPngTileSource.cpp
)

# curl HTTP 字节源：仅当系统/环境中找到 libcurl。
if(MAP_CURL_LIBRARY)
    list(APPEND MAP_PROVIDERS_SOURCES
        providers/CurlBytesSource.h
        providers/CurlBytesSource.cpp
    )
    target_link_libraries(earth_engine_core PRIVATE ${MAP_CURL_LIBRARY})
endif()

# stb/glm/curl 等第三方头（GIS_MD_VCPKG_INSTALLED/include，只读复用）。
target_include_directories(earth_engine_core SYSTEM PRIVATE ${MAP_THIRDPARTY_INCLUDE_DIR})

target_sources(earth_engine_core PRIVATE
    ${MAP_CORE_MATH_SOURCES}
    ${MAP_CORE_GEODESY_SOURCES}
    ${MAP_TILING_SOURCES}
    ${MAP_CONTENT_SOURCES}
    ${MAP_CAMERA_SOURCES}
    ${MAP_PROVIDERS_SOURCES}
)
