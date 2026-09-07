package com.mapcplus.terrain;

import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** A2：把帧绘制交给 native（C++ demo_scene 渲染 core 地形网格）。 */
public class NativeRenderer implements GLSurfaceView.Renderer {
    static {
        System.loadLibrary("terrain_demo");
    }

    private static native void nativeSurfaceCreated();
    private static native void nativeSurfaceChanged(int w, int h);
    private static native void nativeDrawFrame();

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
