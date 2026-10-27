#include "platform/touch_overlay.hpp"

#include "platform/input.hpp"

#ifdef __ANDROID__
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

template <typename Arg>
void CallActivityMethod(const char *name, const char *signature, Arg argument) {
    auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    auto  activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }
    jclass    cls = env->GetObjectClass(activity);
    jmethodID method = cls != nullptr ? env->GetMethodID(cls, name, signature) : nullptr;
    if (method != nullptr) {
        env->CallVoidMethod(activity, method, argument);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (cls != nullptr) {
        env->DeleteLocalRef(cls);
    }
    env->DeleteLocalRef(activity);
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_themoonpeople_chronicle_ChronicleTouchOverlay_nativeSetTouchState(
    JNIEnv *, jobject, jint mask, jfloat left_x, jfloat left_y, jfloat right_x, jfloat right_y) {
    InputTouchOverlayState next{};
    if (g_visible.load()) {
        for (std::size_t i = 0; i < kButtons.size(); ++i) {
            if ((mask & (1 << i)) != 0) {
                next.buttons |= kButtons[i];
            }
        }
        next.left_x = left_x;
        next.left_y = left_y;
        next.right_x = right_x;
        next.right_y = right_y;
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
    CallActivityMethod("setTouchControlsVisible", "(Z)V", static_cast<jboolean>(visible));
}

InputTouchOverlayState TouchOverlayState() {
    std::lock_guard lock(g_state_mutex);
    return g_visible.load() ? g_state : InputTouchOverlayState{};
}

namespace dc::platform {

void TouchOverlayPoll() {
    TouchOverlaySetVisible(InputTouchOverlayVisible());
    InputSetTouchOverlay(TouchOverlayState());
}

void TouchOverlaySetOpacity(float opacity) {
    CallActivityMethod("setTouchControlsOpacity", "(F)V", static_cast<jfloat>(opacity));
}

} // namespace dc::platform
#endif
