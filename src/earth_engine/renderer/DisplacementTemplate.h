#pragma once

#include <cstdint>

#include "IRenderDevice.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/Vec3.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

namespace earth_engine::render {

/// 每瓦位移模板网格（GPU 位移路径的几何核）。
///
/// 语义（与 CPU 烘焙路径的对照，T-E1 记账延续）：
/// - 输出每瓦 (nodesPerEdge+1)² 的**椭球面基准网格**：顶点在瓦覆盖的椭球面上
///   （高度 0），uv = mercator 瓦内归一（北=0 行→顶，v 向南增），法线 = 大地法线；
/// - positions 已 **RTC 到相机附近**（减 cameraEcef，float 精度友好；精确相机
///   相对渲染）；
/// - 高度**不在顶点里**：位移路径在 shader 里采样每瓦高度纹理沿法线抬升
///   （CPU 模板一份可跨瓦复用几何；高度数据只走高度纹理 —— 模板/高度解耦，
///   顶点不再每瓦烘焙 = T-E1 的核心形态）；
/// - MeshUploadData.heights 留空（本模板无高度属性）。
MeshUploadData buildTileDisplacementTemplate(const WebMercatorTileScheme& scheme,
                                             const TileKey& key, const Ellipsoid& ellipsoid,
                                             const Vec3& cameraEcef, int nodesPerEdge);

} // namespace earth_engine::render
