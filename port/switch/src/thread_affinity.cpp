#include <switch.h>

#include <cstdint>

#include "gfx/context.hpp"
#include "kazarov/log.hpp"
#include "switch_main.hpp"

namespace dc::platform {

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
