#pragma once

#include <filesystem>
#include <string>
#include <string_view>

inline std::filesystem::path PathsFromUtf8(std::string_view path) {
#ifdef _WIN32
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
#else
    return std::filesystem::path(path);
#endif
}

inline std::string PathsDisplay(const std::filesystem::path &path) {
#ifdef _WIN32
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
#else
    return path.native();
#endif
}

// Removes --data <dir>, --data=<dir>, --save <dir> and --save=<dir> from argv, keeping the order of
// the rest, and returns the new argc.
int PathsConsumeArgs(int argc, char **argv);

int PathsConsumeArgs(int argc, const char **argv);

// The running executable, symlinks resolved; empty when the system cannot say and argv[0] (seen by
// PathsConsumeArgs) is unknown.
std::filesystem::path PathsExecutable();

void PathsSetDataRoot(const std::filesystem::path &root);

void PathsSetSaveRoot(const std::filesystem::path &root);

const std::filesystem::path &PathsDataRoot();

// Created on first use.
const std::filesystem::path &PathsSaveRoot();
