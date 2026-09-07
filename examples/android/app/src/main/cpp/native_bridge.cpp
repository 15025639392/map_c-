#include <jni.h>

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
