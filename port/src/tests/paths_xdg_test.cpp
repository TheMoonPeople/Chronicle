#include <gtest/gtest.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

#include "app/exitcodes.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"

namespace dc::test {

using namespace dc::app;

namespace fs = std::filesystem;

namespace {

// A data/ or save/ beside darkcloud_tests would win over XDG_DATA_HOME, as it should for a
// developer, so these cases cannot run in such a build directory.
class PathsXdgDarkcloud : public testing::Test {
protected:
    void SetUp() override {
        fs::path beside = PathsExecutable().parent_path();
        if (fs::exists(beside / "data") || fs::exists(beside / "save")) {
            GTEST_SKIP() << "the build directory holds data/ or save/";
        }
    }
};

// An empty working directory and nothing in the environment but XDG_DATA_HOME and HOME.
fs::path Isolate(const char *tag) {
    fs::path dir = fs::canonical(fs::temp_directory_path()) / std::format("dc_xdg_{}_{}", tag, dc::test::ProcessId());
    fs::remove_all(dir);
    fs::create_directories(dir / "work");
    fs::current_path(dir / "work");
#ifdef _WIN32
    dc::test::UnsetEnv("LOCALAPPDATA");
#endif
    dc::test::UnsetEnv("DC_DATA");
    dc::test::UnsetEnv("DC_SAVE");
    dc::test::SetEnv("XDG_DATA_HOME", (dir / "xdg"), 1);
    dc::test::SetEnv("HOME", (dir / "home"), 1);
    return dir;
}

void Leave(const fs::path &dir) {
    fs::current_path(fs::temp_directory_path());
    fs::remove_all(dir);
}

} // namespace

// What a fresh Flatpak install sees on first launch: status 3 before any window, naming the XDG
// directory in the command that fills it.
TEST_F(PathsXdgDarkcloud, DarkcloudNamesTheDirectoryToExtractTo) {
    fs::path dir = Isolate("run");
#ifdef _WIN32
    fs::path executable = PathsExecutable().parent_path() / "darkcloud.exe";
#else
    fs::path executable = PathsExecutable().parent_path() / "darkcloud";
#endif
    fs::path log = dir / "output.txt";
#ifdef _WIN32
    std::string command = std::format(R"(""{}" --headless --frames 1 > "{}" 2>&1")", executable.string(), log.string());
    int         status = std::system(command.c_str());
#else
    std::string command = std::format("'{}' --headless --frames 1 > '{}' 2>&1", executable.string(), log.string());
    int         raw = std::system(command.c_str());
    int         status = WIFEXITED(raw) ? WEXITSTATUS(raw) : 128 + WTERMSIG(raw);
#endif
    std::stringstream text;
    text << std::ifstream(log).rdbuf();
    std::string data = PathsDisplay(fs::path(dir / "xdg/chronicle/data").make_preferred());
    ASSERT_TRUE(status == kExitNoData);
    ASSERT_TRUE(text.str().contains("no game data: " + data + " is not a directory"));
    ASSERT_TRUE(text.str().contains("`dcdata extract <disc image> " + data + "`"));
    Leave(dir);
}

} // namespace dc::test
