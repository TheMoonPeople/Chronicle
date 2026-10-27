#include <malloc.h>
#include <switch.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "gfx/context.hpp"
#include "kazarov/log.hpp"
#include "switch_main.hpp"

namespace dc::platform {
namespace {

struct ClockRate {
    u32    hz = 0;
    Result result = 0;
};

ClockRate ReadClockRate(PcvModuleId module) {
    ClkrstSession session{};
    ClockRate     rate;
    rate.result = clkrstOpenSession(&session, module, 3);
    if (R_SUCCEEDED(rate.result)) {
        rate.result = clkrstGetClockRate(&session, &rate.hz);
        clkrstCloseSession(&session);
    }
    return rate;
}

std::uint64_t IntegerCalibrationRounds(std::uint64_t value) {
    for (unsigned int round = 0; round < 32768; ++round) {
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        value *= 2685821657736338717ULL;
    }
    return value;
}

void LogIntegerCalibration() {
    const char *enabled = std::getenv("DC_PERF_CALIBRATE");
    if (enabled == nullptr || std::strcmp(enabled, "0") == 0) {
        return;
    }

    static volatile std::uint64_t sink = 0x4d595df4d0f33173ULL;
    static const bool             warmed = [] {
        sink = IntegerCalibrationRounds(sink);
        return true;
    }();
    static_cast<void>(warmed);
    const std::uint64_t seed = sink;
    std::uint64_t       scheduled_started = 0;
    std::uint64_t       scheduled_ended = 0;
    const bool          scheduled_available = SwitchReadThreadTicks(scheduled_started);
    const u64           started = armGetSystemTick();
    // Tie the loop to the start read and publish its result before the end read.
    const std::uint64_t value = IntegerCalibrationRounds(seed ^ started);
    sink = value;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const u64    ticks = armGetSystemTick() - started;
    const bool   scheduled_valid = scheduled_available && SwitchReadThreadTicks(scheduled_ended);
    const double wall_ms = static_cast<double>(armTicksToNs(ticks)) / 1000000.0;
    if (scheduled_valid) {
        kz::Logf(kz::LogLevel::Info, "switch",
                 "integer throughput rounds 32768 system_ticks {} wall {:.3f} ms scheduled {:.3f} ms checksum {:#x}",
                 ticks, wall_ms, SwitchThreadTicksMilliseconds(scheduled_ended - scheduled_started), value);
    } else {
        kz::Logf(kz::LogLevel::Info, "switch",
                 "integer throughput rounds 32768 system_ticks {} wall {:.3f} ms scheduled unavailable checksum {:#x}",
                 ticks, wall_ms, value);
    }
}

void LogProcessMemory() {
    const char *enabled = std::getenv("DC_PERF_MEMORY");
    if (enabled == nullptr || std::strcmp(enabled, "0") == 0) {
        return;
    }

    const u64             started = armGetSystemTick();
    u64                   used = 0;
    u64                   total = 0;
    const Result          used_result = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    const Result          total_result = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    const struct mallinfo heap = mallinfo();
    const double          query_ms = static_cast<double>(armTicksToNs(armGetSystemTick() - started)) / 1000000.0;
    kz::Logf(kz::LogLevel::Info, "switch",
             "process memory used {} bytes ({:#x}), total {} bytes ({:#x}); malloc allocated {} free {} bytes "
             "free_blocks {} arena {} bytes query {:.3f} ms",
             used, used_result, total, total_result, heap.uordblks, heap.fordblks, heap.ordblks, heap.arena, query_ms);
}

} // namespace

bool SwitchReadThreadTicks(std::uint64_t &ticks) {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_PERF_THREAD_TICKS");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    static std::atomic<bool> unavailable = false;
    if (!enabled || unavailable.load(std::memory_order_relaxed)) {
        return false;
    }

    u64          running = 0;
    const Result result = svcGetInfo(&running, InfoType_ThreadTickCount, CUR_THREAD_HANDLE, TickCountInfo_Total);
    if (R_FAILED(result)) {
        if (!unavailable.exchange(true, std::memory_order_relaxed)) {
            kz::Logf(kz::LogLevel::Warning, "switch", "scheduler thread tick query failed ({:#x})", result);
        }
        return false;
    }
    static std::atomic_flag announced = ATOMIC_FLAG_INIT;
    if (!announced.test_and_set(std::memory_order_relaxed)) {
        kz::Logf(kz::LogLevel::Info, "switch", "scheduler thread ticks use the {} Hz system counter",
                 armGetSystemTickFreq());
    }
    ticks = running;
    return true;
}

double SwitchThreadTicksMilliseconds(std::uint64_t delta) {
    return static_cast<double>(armTicksToNs(delta)) / 1000000.0;
}

void SwitchLogClockRates() {
    LogIntegerCalibration();
    LogProcessMemory();
    const char *enabled = std::getenv("DC_PERF_CLOCKS");
    if (enabled == nullptr || std::strcmp(enabled, "0") == 0) {
        return;
    }

    const Result initialized = clkrstInitialize();
    ClockRate    cpu{.result = initialized};
    ClockRate    gpu{.result = initialized};
    ClockRate    emc{.result = initialized};
    if (R_SUCCEEDED(initialized)) {
        cpu = ReadClockRate(PcvModuleId_CpuBus);
        gpu = ReadClockRate(PcvModuleId_GPU);
        emc = ReadClockRate(PcvModuleId_EMC);
        clkrstExit();
    }
    kz::Logf(kz::LogLevel::Info, "switch",
             "clock rates CPU {} Hz ({:#x}), GPU {} Hz ({:#x}), EMC {} Hz ({:#x})",
             cpu.hz, cpu.result, gpu.hz, gpu.result, emc.hz, emc.result);
}

void SwitchConfigureThreads() {
    if (gfx::ActiveRenderThreadMode() == gfx::RenderThreadMode::Off) {
        return;
    }

    u64 allowed = 0;
    if (R_FAILED(svcGetInfo(&allowed, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0))) {
        kz::Log(kz::LogLevel::Warning, "switch", "could not query available CPU cores");
        return;
    }

    const u32 current = svcGetCurrentProcessorNumber();
    int       game_core = current < 32 && (allowed & (u64{1} << current)) != 0 ? static_cast<int>(current) : -1;
    int       render_core = -1;
    for (int core = 0; core < 32; ++core) {
        if ((allowed & (u64{1} << core)) == 0) {
            continue;
        }
        if (game_core == -1) {
            game_core = core;
        } else if (core != game_core && render_core == -1) {
            render_core = core;
        }
    }
    if (render_core == -1) {
        kz::Logf(kz::LogLevel::Info, "switch", "one available CPU core (mask {:#x}); using system scheduling", allowed);
        return;
    }

    s32    render_original_core = -1;
    u64    render_original_mask = 0;
    s32    render_priority = -1;
    Result render_query = 0;
    Result render_result = 0;
    gfx::detail::RunOnRenderThread([&] {
        const Handle thread = threadGetCurHandle();
        render_query = svcGetThreadCoreMask(&render_original_core, &render_original_mask, thread);
        if (R_SUCCEEDED(render_query) && render_original_mask <= UINT32_MAX) {
            svcGetThreadPriority(&render_priority, thread);
            render_result = svcSetThreadCoreMask(thread, render_core, u32{1} << render_core);
        }
    });
    if (R_FAILED(render_query)) {
        kz::Logf(kz::LogLevel::Warning, "switch", "could not query render thread placement ({:#x})", render_query);
        return;
    }
    if (render_original_mask > UINT32_MAX) {
        kz::Log(kz::LogLevel::Warning, "switch", "render thread has an unsupported CPU mask");
        return;
    }
    if (R_FAILED(render_result)) {
        kz::Logf(kz::LogLevel::Warning, "switch", "could not place render thread on core {} ({:#x})", render_core,
                 render_result);
        return;
    }

    const Result game_result = svcSetThreadCoreMask(threadGetCurHandle(), game_core, u32{1} << game_core);
    if (R_FAILED(game_result)) {
        Result restore_result = 0;
        gfx::detail::RunOnRenderThread([&] {
            restore_result = svcSetThreadCoreMask(threadGetCurHandle(), render_original_core,
                                                  static_cast<u32>(render_original_mask));
        });
        kz::Logf(kz::LogLevel::Warning, "switch", "could not place game thread on core {} ({:#x})", game_core,
                 game_result);
        if (R_FAILED(restore_result)) {
            kz::Logf(kz::LogLevel::Warning, "switch", "could not restore render thread placement ({:#x})",
                     restore_result);
        }
        return;
    }
    s32          game_actual_core = -1;
    u64          game_actual_mask = 0;
    s32          render_actual_core = -1;
    u64          render_actual_mask = 0;
    s32          game_priority = -1;
    const Result game_query = svcGetThreadCoreMask(&game_actual_core, &game_actual_mask, threadGetCurHandle());
    svcGetThreadPriority(&game_priority, threadGetCurHandle());
    Result render_actual_query = 0;
    gfx::detail::RunOnRenderThread([&] {
        render_actual_query = svcGetThreadCoreMask(&render_actual_core, &render_actual_mask, threadGetCurHandle());
    });
    if (R_FAILED(game_query) || R_FAILED(render_actual_query)) {
        kz::Log(kz::LogLevel::Warning, "switch", "could not verify CPU affinity after placement");
        return;
    }
    kz::Logf(kz::LogLevel::Info, "switch",
             "game core {} mask {:#x}, render core {} mask {:#x} (allowed {:#x}; priorities {}, {})",
             game_actual_core, game_actual_mask, render_actual_core, render_actual_mask, allowed, game_priority,
             render_priority);
}

} // namespace dc::platform
