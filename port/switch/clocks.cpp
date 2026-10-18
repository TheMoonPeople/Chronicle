// The handheld GPU clock. A homebrew title starts with the 307.2 MHz configuration; retail games
// get 384 MHz, and some ask for 460.8 MHz, the configuration video.switch_gpu_boost picks (CPU
// 1020 MHz and memory 1600 MHz, as in the others). Docked is left alone. The system drops the
// setting when the title exits.

#include <switch.h>

#include <cstdio>

namespace {
constexpr u32 kHandheldGpu460 = 0x92220007;
bool          g_apm = false;
u32           g_initial = 0;
} // namespace

void SwitchApplyGpuBoost(bool on) {
    if (!g_apm) {
        if (R_FAILED(apmInitialize())) {
            return;
        }
        g_apm = true;
        apmGetPerformanceConfiguration(ApmPerformanceMode_Normal, &g_initial);
    }
    u32    wanted = on ? kHandheldGpu460 : g_initial;
    u32    current = 0;
    Result rc = apmGetPerformanceConfiguration(ApmPerformanceMode_Normal, &current);
    if (R_SUCCEEDED(rc) && current == wanted) {
        return;
    }
    rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, wanted);
    std::printf("apm: handheld configuration %#x -> %#x (rc %#x)\n", current, wanted, rc);
}
