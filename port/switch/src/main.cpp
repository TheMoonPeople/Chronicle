#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
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
    for (const std::string &word : words) {
        args.push_back(word.c_str());
    }
    args.push_back(nullptr);
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
}

} // namespace dc::platform
