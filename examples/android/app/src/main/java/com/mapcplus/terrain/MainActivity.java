package com.mapcplus.terrain;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

public class MainActivity extends Activity {
    private GLSurfaceView glView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        glView = new GLSurfaceView(this);
        NativeRenderer.setAssetManager(getAssets());
        // S2 三刀：瓦片磁盘缓存根目录（filesDir/tilecache；内存→磁盘→网络三层）。
        NativeRenderer.setFilesDir(getFilesDir().getAbsolutePath() + "/tilecache");
        // GLES 3 上下文（A2 起地形渲染需要）。
        glView.setEGLContextClientVersion(3);
        glView.setRenderer(new NativeRenderer());
        glView.setOnTouchListener(new CameraTouchController());
        setContentView(glView);
    }

    @Override
    protected void onPause() { super.onPause(); glView.onPause(); }
    @Override
    protected void onResume() { super.onResume(); glView.onResume(); }
}
