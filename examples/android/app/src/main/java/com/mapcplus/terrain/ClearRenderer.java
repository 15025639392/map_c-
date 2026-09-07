package com.mapcplus.terrain;

import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** A1 最小验证：GLES clear 一帧（非黑即可）。A2 起替换为地形渲染。 */
public class ClearRenderer implements GLSurfaceView.Renderer {
    private int frameCount = 0;

    @Override public void onSurfaceCreated(GL10 unused, EGLConfig config) {
        GLES30.glClearColor(0.10f, 0.16f, 0.28f, 1.0f);
        android.util.Log.i("map_cplus", "GLES: " + GLES30.glGetString(GLES30.GL_VERSION));
    }
    @Override public void onSurfaceChanged(GL10 unused, int w, int h) {
        GLES30.glViewport(0, 0, w, h);
    }
    @Override public void onDrawFrame(GL10 unused) {
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        if (++frameCount % 120 == 1) {
            android.util.Log.i("map_cplus", "frame=" + frameCount);
        }
    }
}
