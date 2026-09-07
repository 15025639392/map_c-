package com.mapcplus.terrain;

import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.View;

/**
 * 极简相机手势：单指拖动 = 俯仰(pitch)与航向(heading)；双指距离缩放 = 高度(alt)。
 * 相机经纬固定于重庆缙云山（后续轮次再扩展平移）。
 */
public class CameraTouchController implements GLSurfaceView.OnTouchListener {
    // 相机状态（与 native 同步）
    private double lonDeg = 106.44;
    private double latDeg = 29.70;
    private double altMeters = 15000.0;
    private double pitchDeg = 45.0;   // 相对地平线向下
    private double headingDeg = 200.0; // 0=北 顺时针

    private float lastX1, lastY1, lastX2, lastY2;
    private float startDist = -1f;
    private boolean twoFinger = false;
    private boolean dirty = false;

    @Override
    public boolean onTouch(View view, MotionEvent ev) {
        final int pc = ev.getPointerCount();
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_POINTER_UP:
                twoFinger = pc >= 2;
                if (twoFinger) {
                    startDist = dist(ev);
                    lastX1 = ev.getX(0);
                    lastY1 = ev.getY(0);
                } else {
                    startDist = -1f;
                }
                lastX1 = ev.getX(0);
                lastY1 = ev.getY(0);
                return true;
            case MotionEvent.ACTION_DOWN:
                twoFinger = false;
                startDist = -1f;
                lastX1 = ev.getX();
                lastY1 = ev.getY();
                return true;
            case MotionEvent.ACTION_MOVE: {
                if (pc >= 2) {
                    float d = dist(ev);
                    if (startDist > 0f) {
                        double factor = startDist / d;
                        altMeters = clamp(altMeters * factor, 500.0, 200000.0);
                        dirty = true;
                    }
                    startDist = d;
                    // 双指也当作旋转（用平均位移）
                    float cx = (ev.getX(0) + ev.getX(1)) * 0.5f;
                    float cy = (ev.getY(0) + ev.getY(1)) * 0.5f;
                    headingDeg = wrapHeading(headingDeg + (cx - lastX1) * 0.15);
                    pitchDeg = clamp(pitchDeg - (cy - lastY1) * 0.10, 2.0, 88.0);
                    lastX1 = cx;
                    lastY1 = cy;
                    dirty = true;
                } else {
                    float x = ev.getX();
                    float y = ev.getY();
                    headingDeg = wrapHeading(headingDeg + (x - lastX1) * 0.20);
                    pitchDeg = clamp(pitchDeg - (y - lastY1) * 0.12, 2.0, 88.0);
                    lastX1 = x;
                    lastY1 = y;
                    dirty = true;
                }
                if (dirty) {
                    dirty = false;
                    NativeRenderer.updateCamera(lonDeg, latDeg, altMeters, pitchDeg,
                                                headingDeg);
                }
                return true;
            }
            default:
                return true;
        }
    }

    private static float dist(MotionEvent ev) {
        float dx = ev.getX(0) - ev.getX(1);
        float dy = ev.getY(0) - ev.getY(1);
        return (float) Math.sqrt(dx * dx + dy * dy);
    }

    private static double clamp(double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    private static double wrapHeading(double h) {
        h %= 360.0;
        return h < 0 ? h + 360.0 : h;
    }
}
