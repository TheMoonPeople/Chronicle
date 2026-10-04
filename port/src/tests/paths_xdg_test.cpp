#include <gtest/gtest.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

#include "exitcodes.hpp"
#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

// A data/ or save/ beside darkcloud_tests would win over XDG_DATA_HOME, as it should for a
// developer, so these cases cannot run in such a build directory.
class PathsXdg : public testing::Test {
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
    fs::path dir = fs::canonical(fs::temp_directory_path()) / std::format("dc_xdg_{}_{}", tag, getpid());
    fs::remove_all(dir);
    fs::create_directories(dir / "work");
    fs::current_path(dir / "work");
    unsetenv("DC_DATA");
    unsetenv("DC_SAVE");
    setenv("XDG_DATA_HOME", (dir / "xdg").c_str(), 1);
    setenv("HOME", (dir / "home").c_str(), 1);
    return dir;
}

void Leave(const fs::path &dir) {
    fs::current_path(fs::temp_directory_path());
    fs::remove_all(dir);
}

} // namespace

TEST_F(PathsXdg, UsedWhenNothingIsLocal) {
    fs::path dir = Isolate("default");
    ASSERT_TRUE(PathsDataRoot() == dir / "xdg/chronicle/data");
    ASSERT_TRUE(!fs::exists(dir / "xdg/chronicle/data"));
    ASSERT_TRUE(PathsSaveRoot() == dir / "xdg/chronicle/save");
    ASSERT_TRUE(fs::is_directory(dir / "xdg/chronicle/save"));
    ASSERT_TRUE(!fs::exists(dir / "work/save"));
    Leave(dir);
}

TEST_F(PathsXdg, FallsBackToHome) {
    fs::path dir = Isolate("home");
    unsetenv("XDG_DATA_HOME");
    ASSERT_TRUE(PathsDataRoot() == dir / "home/.local/share/chronicle/data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "home/.local/share/chronicle/save");
    ASSERT_TRUE(fs::is_directory(dir / "home/.local/share/chronicle/save"));
    Leave(dir);
}

TEST_F(PathsXdg, IgnoresARelativeValue) {
    fs::path dir = Isolate("relative");
    setenv("XDG_DATA_HOME", "relative", 1);
    ASSERT_TRUE(PathsDataRoot() == dir / "home/.local/share/chronicle/data");
    ASSERT_TRUE(!fs::exists(dir / "work/relative"));
    Leave(dir);
}

TEST_F(PathsXdg, LocalDataKeepsItsSaveBesideIt) {
    fs::path dir = Isolate("local");
    fs::create_directories(dir / "work/data");
    ASSERT_TRUE(PathsDataRoot() == dir / "work/data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "work/save");
    ASSERT_TRUE(fs::is_directory(dir / "work/save"));
    ASSERT_TRUE(!fs::exists(dir / "xdg"));
    Leave(dir);
}

TEST_F(PathsXdg, LocalSaveAloneWins) {
    fs::path dir = Isolate("local_save");
    fs::create_directories(dir / "work/save");
    ASSERT_TRUE(PathsDataRoot() == dir / "xdg/chronicle/data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "work/save");
    ASSERT_TRUE(!fs::exists(dir / "xdg/chronicle/save"));
    Leave(dir);
}

TEST_F(PathsXdg, EnvironmentAndFlagsComeFirst) {
    fs::path dir = Isolate("override");
    setenv("DC_DATA", (dir / "env_data").c_str(), 1);
    setenv("DC_SAVE", (dir / "env_save").c_str(), 1);
    ASSERT_TRUE(PathsDataRoot() == dir / "env_data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "env_save");
    ASSERT_TRUE(!fs::exists(dir / "xdg"));
    Leave(dir);
}

// What a fresh Flatpak install sees on first launch: status 3 before any window, naming the XDG
// directory in the command that fills it.
TEST_F(PathsXdg, DarkcloudNamesTheDirectoryToExtractTo) {
    fs::path          dir = Isolate("run");
    fs::path          executable = PathsExecutable().parent_path() / "darkcloud";
    fs::path          log = dir / "output.txt";
    std::string       command = "'" + executable.string() + "' --headless --frames 1 > '" + log.string() + "' 2>&1";
    int               raw = std::system(command.c_str());
    int               status = WIFEXITED(raw) ? WEXITSTATUS(raw) : 128 + WTERMSIG(raw);
    std::stringstream text;
    text << std::ifstream(log).rdbuf();
    std::string data = (dir / "xdg/chronicle/data").string();
    ASSERT_TRUE(status == kExitNoData);
    ASSERT_TRUE(text.str().find("no game data: " + data + " is not a directory") != std::string::npos);
    ASSERT_TRUE(text.str().find("`dcdata extract <disc image> " + data + "`") != std::string::npos);
    Leave(dir);
}
