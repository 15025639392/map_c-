package com.mapcplus.terrain;

import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.View;

/**
 * 极简相机手势：单指拖动 = 俯仰(pitch)与航向(heading)；双指距离缩放 = 高度(alt)。
 * 相机经纬固定于重庆缙云山（后续轮次再扩展平移）。
 *
 * 两种驱动路径（按 native 模式切换，重启生效）：
 * - nav=0（默认）：Java 直接积分并 updateCamera 绝对位姿（基线手感，逐位保留）；
 * - nav=1（L3 导航）：把手势增量（dx/dy/双指缩放比）送给 native host 管线
 *   （GestureToMotion→CameraMotion→TerrainGroundGuard），native 每帧步进并回灌相机，
 *   支持惯性滑行与贴地防护。
 */
public class CameraTouchController implements GLSurfaceView.OnTouchListener {
    // 相机状态（与 native 同步；仅 nav=0 直连路径使用）
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
        if (NativeRenderer.isNavMode()) {
            return onTouchNav(ev);
        }
        return onTouchDirect(view, ev);
    }

    /** nav=0：绝对位姿直连（基线路径，语义/系数不变）。 */
    private boolean onTouchDirect(View view, MotionEvent ev) {
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

    /** nav=1：手势增量 → native host 管线（native 端惯性/贴地防护，Java 不持位姿）。 */
    private boolean onTouchNav(MotionEvent ev) {
        final int pc = ev.getPointerCount();
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                twoFinger = false;
                startDist = -1f;
                lastX1 = ev.getX();
                lastY1 = ev.getY();
                return true;
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_POINTER_UP:
                twoFinger = pc >= 2;
                lastX1 = centroidX(ev);
                lastY1 = centroidY(ev);
                startDist = twoFinger ? dist(ev) : -1f;
                return true;
            case MotionEvent.ACTION_MOVE: {
                if (pc >= 2) {
                    // 双指：质心拖动 = 平移（pan），张/拢指 = 缩放（native 端同帧组合）。
                    float cx = centroidX(ev);
                    float cy = centroidY(ev);
                    NativeRenderer.navPan(cx - lastX1, cy - lastY1);
                    lastX1 = cx;
                    lastY1 = cy;
                    float d = dist(ev);
                    float scale = 1f;
                    if (startDist > 0f) {
                        scale = d / startDist; // 张指(拉近>1)
                    }
                    startDist = d;
                    if (scale != 1f) {
                        NativeRenderer.navGesture(0.0, 0.0, scale);
                    }
                } else {
                    // 单指：拖动 = 俯仰/航向（旋转轴）。
                    float x = ev.getX();
                    float y = ev.getY();
                    NativeRenderer.navGesture(x - lastX1, y - lastY1, 1.0);
                    lastX1 = x;
                    lastY1 = y;
                }
                return true;
            }
            default:
                // UP/CANCEL：不再送增量 → native 惯性滑行衰减直至收敛。
                return true;
        }
    }

    private static float centroidX(MotionEvent ev) {
        return (ev.getX(0) + ev.getX(1)) * 0.5f;
    }

    private static float centroidY(MotionEvent ev) {
        return (ev.getY(0) + ev.getY(1)) * 0.5f;
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
