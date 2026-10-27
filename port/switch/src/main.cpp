#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
std::uint32_t romfsMountSelf(const char *name);
std::uint32_t romfsUnmount(const char *name);
}

#include "platform/window.hpp"
#include "switch_main.hpp"

namespace dc::platform {

int SwitchMain(int argc, const char **argv, SwitchRunFunction run) {
    setenv("KZ_LIST_ARENA", "1", 0);
    setenv("DC_SHADOW_CLIP_DIRECT", "1", 0);
    setenv("KZ_ALPHA_OPAQUE", "1", 0);
    setenv("KZ_FOG_NEAR_SAFE", "1", 0);
    setenv("KZ_DEPTH_PLANES", "1", 0);
    setenv("DC_HAZE_SKIP_IDENTITY", "1", 0);
    setenv("DC_HAZE_SCALE", "0.5", 0);
    setenv("KZ_FOG_NEAR_RECIPROCAL", "1", 0);
    setenv("KZ_INLINE_MESH_DISPATCH", "1", 0);
    const std::uint32_t romfs_result = romfsMountSelf("romfs");

    std::vector<std::string> words;
    std::ifstream            args_file;
    if (romfs_result == 0) {
        args_file.open("romfs:/args.txt");
    }
    if (!args_file.is_open()) {
        args_file.open("args.txt");
    }
    if (args_file) {
        std::string word;
        while (args_file >> word) {
            words.push_back(std::move(word));
        }
    }
    std::vector<const char *> args;
    if (argc > 0 && argv != nullptr) {
        args.assign(argv, argv + argc);
    }
    if (args.empty()) {
        args.push_back("darkcloud");
    }
    // The console has no environment for the game's settings variables; "--env NAME=VALUE" in args.txt sets one.
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (words[i] == "--env" && i + 1 < words.size()) {
            const std::string &setting = words[++i];
            const std::size_t  equals = setting.find('=');
            if (equals != std::string::npos && equals > 0) {
                setenv(setting.substr(0, equals).c_str(), setting.c_str() + equals + 1, 1);
            }
            continue;
        }
        args.push_back(words[i].c_str());
    }
    args.push_back(nullptr);
    std::fprintf(stderr,
                 "switch: performance settings list_arena=%s shadow_clip_direct=%s alpha_opaque=%s "
                 "fog_near_safe=%s depth_planes=%s haze_skip_identity=%s haze_scale=%s fog_near_reciprocal=%s "
                 "inline_mesh_dispatch=%s\n",
                 std::getenv("KZ_LIST_ARENA"), std::getenv("DC_SHADOW_CLIP_DIRECT"),
                 std::getenv("KZ_ALPHA_OPAQUE"), std::getenv("KZ_FOG_NEAR_SAFE"),
                 std::getenv("KZ_DEPTH_PLANES"), std::getenv("DC_HAZE_SKIP_IDENTITY"),
                 std::getenv("DC_HAZE_SCALE"), std::getenv("KZ_FOG_NEAR_RECIPROCAL"),
                 std::getenv("KZ_INLINE_MESH_DISPATCH"));
    const int result = run(static_cast<int>(args.size()) - 1, args.data());
    if (romfs_result == 0) {
        static_cast<void>(romfsUnmount("romfs"));
    }
    return result;
}

void SwitchConfigureRenderer(gfx::RendererConfig &config) {
    int width = 0;
    int height = 0;
    SwitchDisplaySize(width, height);
    config.render_scale = static_cast<float>(height) / config.logical_height;
    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSizeInPixels(WindowHandle(), &window_width, &window_height)) {
        std::fprintf(stderr, "switch: cannot query SDL output size: %s\n", SDL_GetError());
    }
    std::fprintf(stderr, "switch: mode target=%dx%d, SDL=%dx%d, render scale=%.2f\n", width, height,
                 window_width, window_height, static_cast<double>(config.render_scale));
    SwitchLogClocks();
    SwitchLogClockRates();
}

} // namespace dc::platform
