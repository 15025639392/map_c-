#include <jni.h>
#include <android/asset_manager_jni.h>

#include "demo_scene.h"

namespace {
demoscene::TerrainScene gScene;
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeSurfaceCreated(JNIEnv*, jclass) {
    gScene.initializeGl();
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeSurfaceChanged(JNIEnv*, jclass, jint w, jint h) {
    gScene.resize(static_cast<int>(w), static_cast<int>(h));
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeDrawFrame(JNIEnv*, jclass) {
    gScene.drawFrame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeSetCamera(JNIEnv*, jclass, jdouble lonDeg,
                                                          jdouble latDeg, jdouble altMeters,
                                                          jdouble pitchDeg, jdouble headingDeg) {
    gScene.setCamera(lonDeg, latDeg, altMeters, pitchDeg, headingDeg);
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeSetAssetManager(JNIEnv* env, jclass,
                                                               jobject assetManager) {
    gScene.setAssetManager(AAssetManager_fromJava(env, assetManager));
}
