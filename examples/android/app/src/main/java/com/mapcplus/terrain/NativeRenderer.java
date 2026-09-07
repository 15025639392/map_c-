package com.mapcplus.terrain;

import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** A2：把帧绘制交给 native（C++ demo_scene 渲染 core 地形网格）。 */
public class NativeRenderer implements GLSurfaceView.Renderer {
    static {
        System.loadLibrary("terrain_demo");
    }

    /** 由 MainActivity 传入 APK assets（DEM 资产源用）。 */
    public static void setAssetManager(android.content.res.AssetManager am) {
        nativeSetAssetManager(am);
    }

    private static native void nativeSurfaceCreated();
    private static native void nativeSurfaceChanged(int w, int h);
    private static native void nativeDrawFrame();
    private static native void nativeSetCamera(double lonDeg, double latDeg, double altMeters,
                                               double pitchDeg, double headingDeg);
    private static native void nativeSetAssetManager(android.content.res.AssetManager am);

    /** Java 手势驱动的相机更新（拖动=俯仰/航向，双指=高度）。 */
    public static void updateCamera(double lonDeg, double latDeg, double altMeters,
                                    double pitchDeg, double headingDeg) {
        nativeSetCamera(lonDeg, latDeg, altMeters, pitchDeg, headingDeg);
    }

    @Override public void onSurfaceCreated(GL10 unused, EGLConfig config) {
        nativeSurfaceCreated();
    }
    @Override public void onSurfaceChanged(GL10 unused, int w, int h) {
        nativeSurfaceChanged(w, h);
    }
    @Override public void onDrawFrame(GL10 unused) {
        nativeDrawFrame();
    }
}
