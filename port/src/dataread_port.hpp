#pragma once

#include "common.h"

#include <cstdint>
#include <string_view>
#include <vector>

// The languages the extracted disc carries, as LanguageCode numbers, from the languages.json dcdata
// writes for its release (dcdata::ReleaseOf); none when the data has no such file.
std::vector<s32> DataLanguages();

// The languages the game can offer: the disc's own. A localization pack, once the port has them,
// adds the languages it carries here.
std::vector<s32> SupportedLanguages();

// The size of a file of the game's data ("gedit/e01/chara/p13a.chr"), as the loaders find it; 0 for
// one the data does not have.
std::uintmax_t DataFileSize(std::string_view path);

/**
 * Clears pack bounds for buffers invalidated when the game resets its data arenas.
 *
 */
void DataReadClearPackCapacities();
