package com.mapcplus.terrain;

import android.opengl.GLSurfaceView;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
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

    /**
     * 同步 HTTP(S) GET（native 网络 DEM 源用；由 native GL 线程调用，非主线程）。
     * 非 200 / IO 异常 → null。连接与读超时各 10s。
     */
    public static byte[] httpGetBytes(String url) {
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(10000);
            conn.setRequestProperty("Accept", "image/png,*/*");
            conn.setUseCaches(false);
            final int code = conn.getResponseCode();
            if (code != 200) {
                return null;
            }
            try (InputStream in = conn.getInputStream();
                 ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                final byte[] buf = new byte[16384];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
                return out.toByteArray();
            }
        } catch (Exception e) {
            return null;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    private static native void nativeSurfaceCreated();
    private static native void nativeSurfaceChanged(int w, int h);
    private static native void nativeDrawFrame();
    private static native void nativeSetCamera(double lonDeg, double latDeg, double altMeters,
                                               double pitchDeg, double headingDeg);
    private static native void nativeSetAssetManager(android.content.res.AssetManager am);
    private static native boolean nativeNavEnabled();
    private static native void nativeNavGesture(double dxPx, double dyPx, double pinchScale);
    private static native void nativeNavPan(double dxPx, double dyPx);

    /** Java 手势驱动的相机更新（拖动=俯仰/航向，双指=高度）。 */
    public static void updateCamera(double lonDeg, double latDeg, double altMeters,
                                    double pitchDeg, double headingDeg) {
        nativeSetCamera(lonDeg, latDeg, altMeters, pitchDeg, headingDeg);
    }

    /** L3 导航模式（debug.mapc.nav=1）：手势增量送 native host 管线（惯性+贴地防护）。 */
    public static boolean isNavMode() {
        return nativeNavEnabled();
    }

    /** 手势增量：dx/dy = 屏幕像素（y 向下为正），pinchScale = 本帧双指距离比（>1 拉近）。 */
    public static void navGesture(double dxPx, double dyPx, double pinchScale) {
        nativeNavGesture(dxPx, dyPx, pinchScale);
    }

    /** 双指平移增量（质心像素；native 与旋转/缩放同帧组合）。 */
    public static void navPan(double dxPx, double dyPx) {
        nativeNavPan(dxPx, dyPx);
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
