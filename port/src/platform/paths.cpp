#include "paths.hpp"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::optional<fs::path> data_override;
std::optional<fs::path> save_override;
std::optional<fs::path> data_root;
std::optional<fs::path> save_root;
fs::path                program;

fs::path ExecutableDirectory() { return PathsExecutable().parent_path(); }

std::optional<fs::path> FromEnvironment(const char *name) {
#ifdef _WIN32
    const std::wstring wide_name(name, name + std::strlen(name));
    const wchar_t     *value = ::_wgetenv(wide_name.c_str());
#else
    const char *value = std::getenv(name);
#endif
    if (value && *value) {
        return fs::path(value);
    }
    return std::nullopt;
}

// The working directory first, so a checkout run from its root finds its own data/, then beside
// the executable, so an unpacked copy works from anywhere.
std::optional<fs::path> Local(const char *name) {
    std::error_code error;
    fs::path        here = fs::current_path(error) / name;
    if (!error && fs::is_directory(here, error)) {
        return here;
    }
    fs::path executable = ExecutableDirectory();
    if (!executable.empty() && fs::is_directory(executable / name, error)) {
        return executable / name;
    }
    return std::nullopt;
}

// An installed copy, the Flatpak above all, has nothing local and may not write beside itself.
// Inside the sandbox XDG_DATA_HOME is ~/.var/app/<id>/data, which the app owns without any
// filesystem permission. The XDG spec has a relative XDG_DATA_HOME ignored.
fs::path Installed(const char *name) {
    fs::path base;
#ifdef _WIN32
    if (std::optional<fs::path> local = FromEnvironment("LOCALAPPDATA")) {
        return *local / "chronicle" / name;
    }
#endif
    if (std::optional<fs::path> xdg = FromEnvironment("XDG_DATA_HOME"); xdg && xdg->is_absolute()) {
        base = *xdg;
    } else if (std::optional<fs::path> home = FromEnvironment("HOME")) {
        base = *home / ".local" / "share";
    } else {
        std::error_code error;
        return fs::current_path(error) / name;
    }
    return base / "chronicle" / name;
}

fs::path Absolute(const fs::path &path) {
    std::error_code error;
    fs::path        absolute = fs::absolute(path, error);
    return error ? path : absolute.lexically_normal();
}

} // namespace

fs::path PathsExecutable() {
    std::error_code error;
#if defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        // The dyld path may run through symlinks and "..", as the process was started.
        if (char *real = realpath(path.c_str(), nullptr)) {
            fs::path resolved = real;
            std::free(real);
            return resolved;
        }
    }
#elif defined(_WIN32)
    std::vector<wchar_t> path(32768);
    DWORD                size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size > 0 && size < path.size()) {
        return fs::path(std::wstring_view(path.data(), size));
    }
#elif defined(__linux__)
    fs::path self = fs::read_symlink("/proc/self/exe", error);
    if (!error) {
        return self;
    }
#endif
    if (program.empty()) {
        return {};
    }
    fs::path absolute = fs::absolute(program, error);
    return error ? fs::path() : absolute;
}

int PathsConsumeArgs(int argc, const char **argv) {
    if (argc > 0 && argv[0]) {
        program = PathsFromUtf8(argv[0]);
    }
    int kept = argc > 0 ? 1 : 0;
    for (int i = kept; i < argc; i++) {
        std::string_view         arg = argv[i];
        std::optional<fs::path> *target;
        std::string_view         flag;
        if (arg.starts_with("--data")) {
            target = &data_override;
            flag = "--data";
        } else if (arg.starts_with("--save")) {
            target = &save_override;
            flag = "--save";
        } else {
            argv[kept++] = argv[i];
            continue;
        }
        std::string_view rest = arg.substr(flag.size());
        if (rest.starts_with('=')) {
            *target = PathsFromUtf8(rest.substr(1));
        } else if (rest.empty() && i + 1 < argc) {
            *target = PathsFromUtf8(argv[++i]);
        } else if (rest.empty()) {
            std::fprintf(stderr, "%s needs a directory\n", argv[i]);
            std::exit(2);
        } else {
            argv[kept++] = argv[i];
            continue;
        }
        (target == &data_override ? data_root : save_root).reset();
    }
    if (kept < argc) {
        argv[kept] = nullptr;
    }
    return kept;
}

int PathsConsumeArgs(int argc, char **argv) {
    return PathsConsumeArgs(argc, const_cast<const char **>(argv));
}

void PathsSetDataRoot(const fs::path &root) {
    data_override = root;
    data_root.reset();
}

void PathsSetSaveRoot(const fs::path &root) {
    save_override = root;
    save_root.reset();
}

const fs::path &PathsDataRoot() {
    if (!data_root) {
        std::optional<fs::path> chosen = data_override ? data_override : FromEnvironment("DC_DATA");
        data_root = Absolute(chosen.value_or(Local("data").value_or(Installed("data"))));
    }
    return *data_root;
}

// A save/ that exists wins; otherwise saves go beside a local data/, so a checkout or an unpacked
// copy stays self-contained, and only an installed copy writes under XDG_DATA_HOME.
const fs::path &PathsSaveRoot() {
    if (save_root) {
        return *save_root;
    }
    std::optional<fs::path> chosen = save_override ? save_override : FromEnvironment("DC_SAVE");
    std::vector<fs::path>   candidates;
    if (chosen) {
        candidates.push_back(Absolute(*chosen));
    } else if (std::optional<fs::path> save = Local("save")) {
        candidates.push_back(Absolute(*save));
    } else if (std::optional<fs::path> data = Local("data")) {
        candidates.push_back(Absolute(data->parent_path() / "save"));
        candidates.push_back(Absolute(Installed("save")));
    } else {
        candidates.push_back(Absolute(Installed("save")));
    }
    std::error_code error;
    for (const fs::path &candidate : candidates) {
        if (fs::create_directories(candidate, error), !error) {
            return *(save_root = candidate);
        }
    }
    std::fprintf(stderr, "cannot create the save directory %s: %s\n", PathsDisplay(candidates[0]).c_str(),
                 error.message().c_str());
    return *(save_root = candidates[0]);
}
