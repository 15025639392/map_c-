#include <jni.h>
#include <android/asset_manager_jni.h>

#include <optional>
#include <string>
#include <vector>

#include "demo_scene.h"
#include "net_fetch.h"

namespace {
demoscene::TerrainScene gScene;

JavaVM* gVm = nullptr;
jclass gNativeRendererClass = nullptr;
jmethodID gHttpGetBytesMethod = nullptr;
} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    gVm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jclass local = env->FindClass("com/mapcplus/terrain/NativeRenderer");
    if (local == nullptr) {
        return JNI_ERR;
    }
    gNativeRendererClass = static_cast<jclass>(env->NewGlobalRef(local));
    gHttpGetBytesMethod =
        env->GetStaticMethodID(gNativeRendererClass, "httpGetBytes",
                               "(Ljava/lang/String;)[B");
    env->DeleteLocalRef(local);
    if (gHttpGetBytesMethod == nullptr) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}

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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeNavEnabled(JNIEnv*, jclass) {
    return gScene.navEnabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeNavGesture(JNIEnv*, jclass, jdouble dxPx,
                                                          jdouble dyPx, jdouble pinchScale) {
    gScene.navGesture(static_cast<double>(dxPx), static_cast<double>(dyPx),
                      static_cast<double>(pinchScale));
}

extern "C" JNIEXPORT void JNICALL
Java_com_mapcplus_terrain_NativeRenderer_nativeNavPan(JNIEnv*, jclass, jdouble dxPx,
                                                      jdouble dyPx) {
    gScene.navPan(static_cast<double>(dxPx), static_cast<double>(dyPx));
}

namespace demoscene {

std::optional<std::vector<uint8_t>> httpGetBytes(const std::string& url) {
    if (gVm == nullptr || gNativeRendererClass == nullptr || gHttpGetBytesMethod == nullptr) {
        return std::nullopt;
    }
    JNIEnv* env = nullptr;
    if (gVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || env == nullptr) {
        return std::nullopt; // 只允许在已附着线程（GL 渲染线程）调用
    }
    jstring jurl = env->NewStringUTF(url.c_str());
    if (jurl == nullptr) {
        return std::nullopt;
    }
    const jbyteArray arr = static_cast<jbyteArray>(
        env->CallStaticObjectMethod(gNativeRendererClass, gHttpGetBytesMethod, jurl));
    env->DeleteLocalRef(jurl);
    if (arr == nullptr) {
        return std::nullopt;
    }
    const jsize n = env->GetArrayLength(arr);
    std::vector<uint8_t> out(static_cast<size_t>(n));
    if (n > 0) {
        env->GetByteArrayRegion(arr, 0, n, reinterpret_cast<jbyte*>(out.data()));
    }
    env->DeleteLocalRef(arr);
    return out;
}

} // namespace demoscene
