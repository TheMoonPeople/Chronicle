#include <SDL3/SDL.h>

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "platform/window.hpp"
#include "switch_main.hpp"

int SwitchMain(int argc, const char **argv, SwitchRunFunction run) {
    std::vector<std::string> words;
    if (std::ifstream file("args.txt"); file) {
        std::string word;
        while (file >> word) {
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
    return run(static_cast<int>(args.size()) - 1, args.data());
}

void SwitchConfigureRenderer(gfx::RendererConfig &config) {
    int width = 0;
    int height = 0;
    if (SDL_GetWindowSizeInPixels(WindowHandle(), &width, &height) && height > 0) {
        config.render_scale = static_cast<float>(height) / config.logical_height;
    }
}
