#include "identity.hpp"

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#endif

#include "icon/chronicle.hpp"
#include "icon_retail.hpp"
#include "platform/game_icon.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"
#include "platform/window.hpp"

void GameSetIdentity() {
    SettingsSetEnvPrefix("DC");
    PathsConfigure({.app_dir = "chronicle", .env_data = "DC_DATA", .env_save = "DC_SAVE"});
#if defined(__ANDROID__)
    const char *private_root = SDL_GetAndroidInternalStoragePath();
    if (private_root != nullptr) {
        PathsSetDataRoot(std::filesystem::path(private_root) / "data");
        PathsSetSaveRoot(std::filesystem::path(private_root) / "save");
    }
#endif
#if defined(__ANDROID__)
    WindowSetIdentity({ .title = "Chronicle: Dark Cloud",
#else
    WindowSetIdentity({.title = "Dark Cloud",
#endif
                        .app_id = "dcdecomp.darkcloud",
                        .icon_rgba = kIconRgba,
                        .icon_size = kIconSize });
}

void GameSetDataIcon() {
    // The save menu's pack in each language folder (menu_draw.cpp's ComMenuContryName) carries the memory
    // card icon; a disc holds only its own languages, so every folder is tried, English first.
    GameIconSource source = RetailIconSource(PathsDataRoot());
    source.ico_file = L"darkcloud.ico";
    const std::wstring shortcut = (PathsDataRoot().parent_path() / "Dark Cloud").wstring();
    source.shortcut = shortcut.c_str();
    GameIconInstall(PathsDataRoot(), source);
}
