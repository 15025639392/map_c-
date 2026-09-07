# source-index.md — 符号 → 文件（本仓轻量索引，随实现扩充）

> 对应 stage6-merge-checkpoint.md 并入步骤 6。行号无守卫，以符号名为准；
> 与 gis-md 的 AI_INDEX 分工不同——这里只给"在哪"，不背"行号漂移红"。
> 更新纪律：新增顶层公共符号时顺手补一行。

## core/math
| 符号 | 文件 |
|---|---|
| kPi / degreesToRadians / wrapLongitude / equalsEpsilon … | core/math/MathUtils.h |
| Vec3 | core/math/Vec3.h |
| Vec2 | core/math/Vec2.h |
| Mat4（列主序，Gauss-Jordan 逆） | core/math/Mat4.{h,cpp} |
| Ray | core/math/Ray.h |
| RayTriangle（rayTriangleIntersection） | core/math/RayTriangle.h |
| Rectangle（弧度制） | core/math/Rectangle.h |
| Plane | core/math/Plane.h |

## core/geodesy
| 符号 | 文件 |
|---|---|
| Cartographic | core/geodesy/Cartographic.h |
| Ellipsoid（WGS84 双向转换/法线/scaleToGeodeticSurface） | core/geodesy/Ellipsoid.{h,cpp} |
| Transforms::eastNorthUpToFixedFrame / fixedFrameToEastNorthUp | core/geodesy/Transforms.{h,cpp} |
| RayEllipsoid（intersectRayEllipsoid / firstRayEllipsoidIntersection） | core/geodesy/RayEllipsoid.{h,cpp} |
| Projection / GeographicProjection / WebMercatorProjection | core/geodesy/Projection.{h,cpp} |
| QuadtreeGeometricError::screenSpaceError / shouldRefine | core/geodesy/QuadtreeGeometricError.{h,cpp} |

## tiling
| 符号 | 文件 |
|---|---|
| TileKey（z/x/y + parent/children/ancestor + hash） | tiling/TileKey.h |
| WebMercatorTileScheme（XYZ 顶行原点；origin/size/rect/点→键） | tiling/WebMercatorTileScheme.{h,cpp} |
| TerrainLodSelector / TerrainLodConfig / TerrainLodResult | tiling/TerrainLodSelector.{h,cpp} |

## content
| 符号 | 文件 |
|---|---|
| HeightmapCodec（Terrain-RGB / Terrarium 编解码；kTerrainRgbNoDataFloorMeters 哨兵常量） | content/HeightmapCodec.{h,cpp} |
| HeightmapSampler（nearest / bilinear CLAMP；可选 no-data 哨兵 → 哨兵角归一化） | content/HeightmapSampler.{h,cpp} |
| HeightmapTile（mercator 米查高，像素↔地理；min/max 排除哨兵） | content/HeightmapTile.{h,cpp} |
| TerrainMeshData / TerrainTileMeshBuilder | content/TerrainTileMesh.{h,cpp} |
| ITerrainDataSource / TerrainGrid（heights + noDataValues + borderInset） | content/TerrainDataSource.h |
| AncestorFallbackDataSource（缺瓦祖先回退 + 重采样，调度-lite） | content/AncestorFallbackDataSource.{h,cpp} |
| HeightDatumCorrectingDataSource（undulation 逐样本叠加，EGM96 接入路径） | content/HeightDatumCorrectingDataSource.{h,cpp} |
| TerrainFrameAssembler | content/TerrainFrameAssembler.{h,cpp} |
| auditSameLevelSharedEdges / SeamAuditResult（同级共享边审计） | content/SeamAudit.{h,cpp} |
| auditCrossLevelTVertexGap（跨级 T-顶点裂缝度量，B4 前身） | content/SeamAudit.{h,cpp} |
| TerrainFrameCache（增量/淘汰） | content/TerrainFrameCache.{h,cpp} |
| pickTerrainFrame / TerrainPickHit | content/TerrainPicking.{h,cpp} |

## camera
| 符号 | 文件 |
|---|---|
| CameraView（位姿基 / rayThroughNdc / groundFootprintRadians） | camera/CameraView.{h,cpp} |
| Frustum（fromCamera / containsPoint / intersectsSphere） | camera/Frustum.{h,cpp} |
| assembleTerrainFrameForCamera / TerrainCameraPipelineConfig | camera/TerrainCameraPipeline.{h,cpp} |

## providers
| 符号 | 文件 |
|---|---|
| TileUrlFormatter（{z}/{x}/{y}） | providers/TileUrlFormatter.{h,cpp} |
| ITileBytesSource | providers/ITileBytesSource.h |
| TerrainRgbTileSource（RGB 行） | providers/TerrainRgbTileSource.{h,cpp} |
| StbPngDecoder / decodePngToRgb | providers/StbPngDecoder.{h,cpp} |
| TerrainRgbPngTileSource（PNG 瓦片） | providers/TerrainRgbPngTileSource.{h,cpp} |
| CurlBytesSource（HTTP） | providers/CurlBytesSource.{h,cpp} |
| ImageTileBodyCheck（响应体魔数白名单 PNG/JPEG/WebP） | providers/ImageTileBodyCheck.h |

## 测试 → 模块（tests/unit/…）
`core/`：mat4、math_utils、ray、ray_triangle、rectangle、vec2、vec3；
`geodesy/`：cartographic、ellipsoid、projection、quadtree_geometric_error、ray_ellipsoid、transforms；
`tiling/`：tile_key、tile_scheme、terrain_lod_selector；
`content/`：heightmap_codec、heightmap_sampler、heightmap_tile、terrain_tile_mesh、
terrain_frame_assembler、ancestor_fallback（祖先回退/调度-lite）、seam_audit、
ring_source_seam（B2 环源闭合）、cross_level_tvertex（B4 T-顶点取证）、
terrain_cross_level、terrain_picking、
terrain_frame_cache、decode_nodata_semantics（gis-md 哨兵语义对拍）、
fixed_station_baseline；
`camera/`：camera_view、frustum、terrain_camera_pipeline；
`providers/`：tile_url_formatter、terrain_rgb_source、png_terrain_source、http_bytes_source、
terrarium_asset_decode（真实资产字节回归，Terrarium 路径 + T-V5 真数据账本）。
