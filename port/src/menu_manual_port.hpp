#pragma once

#include <string>
#include <utility>
#include <vector>

/**
 * Returns the journal's translation keys, each paired with its English default text, for the
 * options screen's table of port strings.
 */
std::vector<std::pair<std::string, std::string>> JournalStrings();
