#pragma once

#include <string>
#include <vector>

#include "fishing/framework.hpp"

/**
 * What quests ask of fishing, kept in the quest's own numbers (quest::VarAtLeast reads them): each
 * helper follows the catches from when it is called, usually as the quest is registered, and counts
 * those that match into a var of the quest, saved with its progress.
 */
namespace dc::fishing {

/**
 * Which catches count; an empty field matches any.
 */
struct CatchFilter {
    std::string species;      /**< vanilla:fish_17 */
    std::string place;        /**< vanilla:matataki_2 */
    std::string bait;         /**< vanilla:poisonous_apple */
    int         min_size = 0; /**< In the records' units, tenths of a size unit. */

    [[nodiscard]] bool Matches(const Catch &fish) const;
};

/**
 * The quest's stages a helper counts at; empty for any.
 */
using Stages = std::vector<std::string>;

/**
 * Adds 1 to quest's var for each catch filter matches.
 */
void CountCatches(std::string quest, std::string var, CatchFilter filter = {}, Stages while_at = {});

/**
 * Adds each catch's points to quest's var.
 */
void CountFishPoints(std::string quest, std::string var);

/**
 * Keeps in quest's var the size of the largest catch filter matches.
 */
void KeepLargest(std::string quest, std::string var, CatchFilter filter = {});

/**
 * Counts in quest's var how many of species have been caught, each once (marked in var.<species>):
 * "catch every fish of Norune" is CountSpecies with Norune's four and VarAtLeast(var, 4).
 */
void CountSpecies(std::string quest, std::string var, std::vector<std::string> species, Stages while_at = {});

/**
 * Whether the player has caught a fish yet, in this save: retail's records, the fish points, or a
 * custom species' record.
 */
bool HasCaughtAFish();

} // namespace dc::fishing
