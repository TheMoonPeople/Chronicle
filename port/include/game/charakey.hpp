#pragma once

/**
 * @file
 * The per-character special-attack key handlers for Toan, Ungaga and Goro.
 */

#include "common.h"

namespace dc::game {

/**
 * Starts Toan's special attack.
 */
void ToanKey_On();

/**
 * Steps Toan's special attack by one frame.
 */
void ToanKey_Play();

/**
 * Starts Ungaga's special attack.
 */
void UngagaKey_On();

/**
 * Steps Ungaga's special attack by one frame.
 */
void UngagaKey_Play();

/**
 * Starts Goro's special attack.
 */
void GoroKey_On();

/**
 * Steps Goro's special attack by one frame.
 */
void GoroKey_Play();

} // namespace dc::game
