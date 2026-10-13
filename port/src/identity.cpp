#include "identity.hpp"

#include "icon/chronicle.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"
#include "platform/window.hpp"

void GameSetIdentity() {
    SettingsSetEnvPrefix("DC");
    PathsConfigure({.app_dir = "chronicle", .env_data = "DC_DATA", .env_save = "DC_SAVE"});
    WindowSetIdentity({.title = "Dark Cloud",
                       .app_id = "dcdecomp.darkcloud",
                       .icon_rgba = kIconRgba,
                       .icon_size = kIconSize});
}
