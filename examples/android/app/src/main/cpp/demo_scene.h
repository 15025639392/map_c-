#pragma once

#include <android/asset_manager.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <earth_engine/camera/MapCameraSystem.h>
#include <earth_engine/core/math/Vec3.h>
#include <earth_engine/content/TerrainDataSource.h>
#include <earth_engine/interaction/PointerGestureRecognizer.h>
#include <earth_engine/providers/DiskTileCacheBytesSource.h>
#include <earth_engine/providers/TileCacheBytesSource.h>
#include <earth_engine/renderer/IRenderDevice.h>
#include <earth_engine/scene/LayerStack.h>

#include "dem_assets.h"

namespace demoscene {

/// A2/A3 场景：相机 → core 管线（选择/解码/网格）→ IRenderDevice(GLES3) 渲染。
/// 相机初始 = station 预设（debug.mapc.station 1..5），Java 手势可改写。
/// L3：相机位姿一律由**引擎层 MapCameraSystem** 持有（本类只做平台数据腿/渲染消费）：
/// - nav=0（默认）：Java 手势绝对位姿 → setCamera → 引擎 setPose（基线路径不动）；
/// - nav=1（debug.mapc.nav）：Java 只送屏幕手势增量，引擎做惯性/贴地防护/flyTo，
///   每帧把引擎位姿回灌场景（turret 语义：正下中心经纬 + 高度 + heading/pitch）。
class TerrainScene {
public:
    void initializeGl();
    void resize(int widthPx, int heightPx);
    void setCamera(double lonDeg, double latDeg, double altMeters, double pitchDeg,
                   double headingDeg);
    void setAssetManager(AAssetManager* manager);
    void drawFrame();

    /// L3 导航（debug.mapc.nav=1）：手势增量输入（Java 手势线程调用；GL 线程消费）。
    void navGesture(double dxPx, double dyPx, double pinchScale);
    /// L3 导航：双指平移增量（质心像素；与旋转/缩放同帧可组合）。
    void navPan(double dxPx, double dyPx);
    /// L3 导航：原始触摸流转发（Java MotionEvent → 引擎手势识别器，GL 帧消费）。
    /// action: 0=Down 1=Move 2=PointerDown 3=PointerUp 4=Up 5=Cancel；pIdx 为事件目标指。
    void navTouchEvent(int action, int pointerIndex, const float* xs, const float* ys, int n);
    /// S2 三刀：瓦片磁盘缓存根目录（Java filesDir/tilecache；≤0/空 = 不启用磁盘层）。
    void setNavCacheRoot(const std::string& root);
    bool navEnabled() const { return navEnabled_; }

private:
    void ensureGeometry();   // 相机→选择→装配，并把网格上传 GPU
    void destroyGlObjects();
    void navStep();          // GL 帧：drain 手势 → 引擎 MapCameraSystem.step → 回灌位姿
    std::optional<double> guardGroundHeightRad(double lonRad, double latRad);

    std::unique_ptr<earth_engine::render::IRenderDevice> device_;
    uint32_t programHandle_ = 0;
    uint32_t textureHandle_ = 0; // 合成"影像瓦"棋盘（纹理管线验证）
    // 每瓦一个设备网格句柄（DrawList-lite：逐瓦上传/绘制/账本到瓦级）。
    std::vector<uint32_t> meshHandles_;
    // NASA 模式的每瓦真实纹理（与 meshHandles_ 对齐；0 = 缺失回退高度着色）。
    std::vector<uint32_t> tileTextures_;
    // 高德路网注记叠加层（与 meshHandles_ 对齐；0 = 无标注）。
    std::vector<uint32_t> labelTextures_;
    bool geometryReady_ = false;
    int width_ = 1080;
    int height_ = 2400;
    int station_ = 2; // debug.mapc.station: 1=M-near 2=M-mid 3=M-graze 4=M-high 5=M-coarse
    long frameCount_ = 0;
    long tilesDrawn_ = 0;
    int totalVertices_ = 0;

    earth_engine::Vec3 cameraPosCache_;
    earth_engine::Vec3 cameraTargetCache_;
    earth_engine::Vec3 cameraUpCache_;

    // 活动相机（station 预设为初值；手势改）——与引擎 MapCameraSystem 位姿同步。
    double camLonDeg_ = 106.44;
    double camLatDeg_ = 29.70;
    double camAltMeters_ = 15000.0;
    double camPitchDeg_ = 45.0;    // 相对地平线向下角（0=掠视 90=正下）
    double camHeadingDeg_ = 200.0; // 0=北 顺时针
    bool cameraUserSet_ = false;
    bool useDem_ = false; // debug.mapc.dem=1 → DEM（assets 或 NASA 网络源）
    bool useImg_ = true;  // debug.mapc.img=1 → 卫星影像层（默认开）
    bool useLbl_ = true;  // debug.mapc.lbl=1 → 路网注记层（默认开）
    bool useHgt_ = false; // debug.mapc.hgt=1 → 高度纹理色带（GPU 位移数据链演示）
    bool useDisp_ = false; // debug.mapc.disp=1 → 基准模板+位移向量属性（GPU 顶点位移）
    AAssetManager* assetManager_ = nullptr;
    double lastKey_[5] = {0, 0, 0, 0, 0};
    // S3：引擎图层栈（图层开关的事实源；绘制序 dem→imagery→label→debug）。
    earth_engine::scene::LayerStack layerStack_;

    // S2/S2三刀：字节缓存三层链 = 内存(TileCacheBytesSource) → 磁盘(DiskTileCacheBytesSource)
    // → 网络(NasaHttpBytesSource)。成员声明序 = 构造序；析构逆序，故 raw 必须最先声明
    // （最后析构），disk 次之，mem 最后声明最先析构——引用链生命周期安全。
    std::unique_ptr<NasaHttpBytesSource> rawBytesSource_;
    std::unique_ptr<earth_engine::DiskTileCacheBytesSource> ringDiskCache_;
    std::unique_ptr<earth_engine::DiskTileCacheBytesSource> amapDiskCache_;
    std::unique_ptr<earth_engine::DiskTileCacheBytesSource> labelDiskCache_;
    std::unique_ptr<earth_engine::TileCacheBytesSource> ringBytesCache_;
    std::unique_ptr<earth_engine::TileCacheBytesSource> amapBytesCache_;
    std::unique_ptr<earth_engine::TileCacheBytesSource> labelBytesCache_;
    std::string navCacheRoot_; // <files>/tilecache（Java 传入）

    // S2 二刀：逐瓦 GL 纹理句柄持久映射（重建复用；避免每重建解码+新建+泄漏）。
    uint32_t textureForKey(std::map<earth_engine::TileKey, uint32_t>& map,
                           std::vector<earth_engine::TileKey>& order, const earth_engine::TileKey& key,
                           const std::function<uint32_t()>& create, size_t cap);
    std::map<earth_engine::TileKey, uint32_t> imgTexByTile_;
    std::map<earth_engine::TileKey, uint32_t> lblTexByTile_;
    std::map<earth_engine::TileKey, uint32_t> hgtTexByTile_;
    std::vector<earth_engine::TileKey> imgTexOrder_;
    std::vector<earth_engine::TileKey> lblTexOrder_;
    std::vector<earth_engine::TileKey> hgtTexOrder_;

    // L3 导航（引擎 MapCameraSystem；默认关 → 基线手势直连不变）。
    bool navEnabled_ = false;
    earth_engine::MapCameraSystem navCam_;
    std::mutex navMutex_;
    double navDxPx_ = 0.0;
    double navDyPx_ = 0.0;
    double navScale_ = 1.0;
    bool navHasInput_ = false;
    double navPanDxPx_ = 0.0;
    double navPanDyPx_ = 0.0;
    bool navPanHas_ = false;
    double navLastStepMs_ = 0.0;
    // 引擎手势识别器 + 原始触摸事件队列（Java 线程入队、GL 帧消费）。
    earth_engine::interaction::PointerGestureRecognizer navRecognizer_;
    struct RawTouchEvent {
        int action = 0;
        int pointerIndex = 0;
        std::vector<double> xs;
        std::vector<double> ys;
    };
    std::vector<RawTouchEvent> navTouchQueue_;
    std::string flyProp_; // debug.mapc.flyto="lon,lat,alt,pit,hdg" 触发一次引擎 flyTo
    std::string panProbeProp_; // debug.mapc.panprobe="dx,dy" 注入 ~45 帧平移（设备证据）
    double panProbeDx_ = 0.0;
    double panProbeDy_ = 0.0;
    int panProbeFrames_ = 0;
    bool panProbeArmed_ = false;
    double panProbeStartLon_ = 0.0;
    double panProbeStartLat_ = 0.0;
    // 贴地防护查高缓存：相机正下（中心经纬固定）所在瓦的栅格。
    std::optional<earth_engine::TileKey> guardKey_;
    earth_engine::TerrainGrid guardGrid_;
    int guardLevel_ = 12;
};

} // namespace demoscene
