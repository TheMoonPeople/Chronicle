#pragma once

/**
 * Chooses what the dungeon's next quick-change menu shows: the party (SELECT, as retail) or the
 * element picker (D-pad Up), whose cells are the elements the equipped weapon can take.
 */
void QuickChangeOpenElements(bool elements);

/**
 * Whether the equipped weapon has more than one element to pick between, so the picker would
 * offer a choice.
 */
bool ElementPickerHasChoice();
