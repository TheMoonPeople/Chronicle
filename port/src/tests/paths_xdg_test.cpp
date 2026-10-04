#include <gtest/gtest.h>
#include <stdlib.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

#include "exitcodes.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"

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
    dc::test::UnsetEnv("XDG_DATA_HOME");
    ASSERT_TRUE(PathsDataRoot() == dir / "home/.local/share/chronicle/data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "home/.local/share/chronicle/save");
    ASSERT_TRUE(fs::is_directory(dir / "home/.local/share/chronicle/save"));
    Leave(dir);
}

TEST_F(PathsXdg, IgnoresARelativeValue) {
    fs::path dir = Isolate("relative");
    dc::test::SetEnv("XDG_DATA_HOME", "relative", 1);
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
    dc::test::SetEnv("DC_DATA", (dir / "env_data"), 1);
    dc::test::SetEnv("DC_SAVE", (dir / "env_save"), 1);
    ASSERT_TRUE(PathsDataRoot() == dir / "env_data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "env_save");
    ASSERT_TRUE(!fs::exists(dir / "xdg"));
    Leave(dir);
}

// What a fresh Flatpak install sees on first launch: status 3 before any window, naming the XDG
// directory in the command that fills it.
TEST_F(PathsXdg, DarkcloudNamesTheDirectoryToExtractTo) {
    fs::path dir = Isolate("run");
#ifdef _WIN32
    fs::path executable = PathsExecutable().parent_path() / "darkcloud.exe";
#else
    fs::path executable = PathsExecutable().parent_path() / "darkcloud";
#endif
    fs::path log = dir / "output.txt";
#ifdef _WIN32
    std::string command = "\"\"" + executable.string() + "\" --headless --frames 1 > \"" + log.string() + "\" 2>&1\"";
    int         status = std::system(command.c_str());
#else
    std::string command = "'" + executable.string() + "' --headless --frames 1 > '" + log.string() + "' 2>&1";
    int         raw = std::system(command.c_str());
    int         status = WIFEXITED(raw) ? WEXITSTATUS(raw) : 128 + WTERMSIG(raw);
#endif
    std::stringstream text;
    text << std::ifstream(log).rdbuf();
    std::string data = PathsDisplay(fs::path(dir / "xdg/chronicle/data").make_preferred());
    ASSERT_TRUE(status == kExitNoData);
    ASSERT_TRUE(text.str().find("no game data: " + data + " is not a directory") != std::string::npos);
    ASSERT_TRUE(text.str().find("`dcdata extract <disc image> " + data + "`") != std::string::npos);
    Leave(dir);
}

#ifdef _WIN32
TEST_F(PathsXdg, LocalAppDataComesBeforeXdg) {
    fs::path dir = Isolate("localappdata");
    dc::test::SetEnv("LOCALAPPDATA", dir / "appdata", 1);
    ASSERT_TRUE(PathsDataRoot() == dir / "appdata/chronicle/data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "appdata/chronicle/save");
    ASSERT_TRUE(fs::is_directory(dir / "appdata/chronicle/save"));
    ASSERT_TRUE(!fs::exists(dir / "xdg"));
    Leave(dir);
}
#endif

#ifdef _WIN32
TEST_F(PathsXdg, LocalAppDataPreservesUnicode) {
    fs::path dir = Isolate("unicode_appdata");
    fs::path base = dir / L"missing-\u6d4b\u8bd5";
    ASSERT_EQ(dc::test::SetEnv("LOCALAPPDATA", base, 1), 0);
    ASSERT_EQ(PathsDataRoot(), base / "chronicle/data");
    ASSERT_EQ(PathsSaveRoot(), base / "chronicle/save");
    ASSERT_TRUE(fs::is_directory(base / "chronicle/save"));
    Leave(dir);
}

TEST_F(PathsXdg, OverridesPreserveUnicode) {
    fs::path dir = Isolate("unicode_overrides");
    fs::path base = dir / L"\u6d4b\u8bd5";
    ASSERT_EQ(dc::test::SetEnv("DC_DATA", base / "data", 1), 0);
    ASSERT_EQ(dc::test::SetEnv("DC_SAVE", base / "save", 1), 0);
    ASSERT_EQ(PathsDataRoot(), base / "data");
    ASSERT_EQ(PathsSaveRoot(), base / "save");
    ASSERT_TRUE(fs::is_directory(base / "save"));
    Leave(dir);
}

TEST_F(PathsXdg, XdgPreservesUnicode) {
    fs::path dir = Isolate("unicode_xdg");
    fs::path base = dir / L"\u6d4b\u8bd5";
    ASSERT_EQ(dc::test::SetEnv("XDG_DATA_HOME", base, 1), 0);
    ASSERT_EQ(PathsDataRoot(), base / "chronicle/data");
    ASSERT_EQ(PathsSaveRoot(), base / "chronicle/save");
    Leave(dir);
}

TEST_F(PathsXdg, HomePreservesUnicode) {
    fs::path dir = Isolate("unicode_home");
    fs::path base = dir / L"\u6d4b\u8bd5";
    dc::test::UnsetEnv("XDG_DATA_HOME");
    ASSERT_EQ(dc::test::SetEnv("HOME", base, 1), 0);
    ASSERT_EQ(PathsDataRoot(), base / ".local/share/chronicle/data");
    ASSERT_EQ(PathsSaveRoot(), base / ".local/share/chronicle/save");
    Leave(dir);
}

TEST_F(PathsXdg, FlagsPreserveUtf8) {
    fs::path    dir = Isolate("unicode_flags");
    fs::path    base = dir / L"\u6d4b\u8bd5";
    std::string data = PathsDisplay(base / "data");
    std::string save = "--save=" + PathsDisplay(base / "save");
    const char *argv[] = {"game", "--data", data.c_str(), save.c_str(), "--offscreen", nullptr};
    ASSERT_EQ(PathsConsumeArgs(5, argv), 2);
    ASSERT_EQ(std::string_view(argv[1]), "--offscreen");
    ASSERT_EQ(PathsDataRoot(), base / "data");
    ASSERT_EQ(PathsSaveRoot(), base / "save");
    Leave(dir);
}
#endif
