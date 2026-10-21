#include "platform/input.hpp"

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <array>
#include <atomic>
#include <mutex>

namespace {
std::mutex                              g_state_mutex;
InputTouchOverlayState                  g_state{};
std::atomic<bool>                       g_visible{true};
constexpr std::array<std::uint16_t, 14> kButtons = {
    kInputUp, kInputDown, kInputLeft, kInputRight,
    kInputCross, kInputCircle, kInputSquare, kInputTriangle,
    kInputL1, kInputR1, kInputL2, kInputR2, kInputSelect, kInputStart};
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_themoonpeople_chronicle_ChronicleTouchOverlay_nativeSetTouchState(
    JNIEnv *, jobject, jint mask, jfloat lx, jfloat ly, jfloat rx, jfloat ry) {
    InputTouchOverlayState next{};
    if (g_visible.load()) {
        for (std::size_t i = 0; i < kButtons.size(); ++i) {
            if (mask & (1 << i)) {
                next.buttons |= kButtons[i];
            }
        }
        next.left_x = lx;
        next.left_y = ly;
        next.right_x = rx;
        next.right_y = ry;
    }
    std::lock_guard lock(g_state_mutex);
    g_state = next;
}

void TouchOverlaySetVisible(bool visible) {
    if (g_visible.exchange(visible) == visible) {
        return;
    }
    if (!visible) {
        std::lock_guard lock(g_state_mutex);
        g_state = {};
    }
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        return;
    }
    jclass    cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "setTouchControlsVisible", "(Z)V") : nullptr;
    if (method) {
        env->CallVoidMethod(activity, method, static_cast<jboolean>(visible));
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (cls) {
        env->DeleteLocalRef(cls);
    }
    env->DeleteLocalRef(activity);
}

InputTouchOverlayState TouchOverlayState() {
    std::lock_guard lock(g_state_mutex);
    return g_visible.load() ? g_state : InputTouchOverlayState{};
}

void TouchOverlayPoll() {
    TouchOverlaySetVisible(InputTouchOverlayVisible());
    InputSetTouchOverlay(TouchOverlayState());
}

void TouchOverlaySetOpacity(float opacity) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        return;
    }
    jclass    cls = env->GetObjectClass(activity);
    jmethodID method = cls ? env->GetMethodID(cls, "setTouchControlsOpacity", "(F)V") : nullptr;
    if (method) {
        env->CallVoidMethod(activity, method, static_cast<jfloat>(opacity));
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (cls) {
        env->DeleteLocalRef(cls);
    }
    env->DeleteLocalRef(activity);
}
#endif
