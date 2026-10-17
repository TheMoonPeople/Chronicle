#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ui_framework/types.hpp"
#include "ui_framework/text.hpp"
#include "ui_framework/surfaces.hpp"
#include "ui_framework/widgets.hpp"
#include "ui_framework/buttons.hpp"
#include "ui_framework/dialogs.hpp"

namespace UI {

std::vector<std::pair<std::string, std::string>> BattleMenuStrings();
std::vector<std::pair<std::string, std::string>> JournalStrings();

} // namespace UI