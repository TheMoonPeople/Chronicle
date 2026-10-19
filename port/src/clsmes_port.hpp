#pragma once

class ClsMes;

// Marks mes as the dialog framework's choice window (dialog_npc.cpp), or none with nullptr. Its rows
// above first_row are text, not choices: with a cursor_row the window dims every row the cursor is not
// on, and these keep their colour. A coloured choice the cursor is not on fades rather than brightens,
// and the hand follows the mouse pointer while one is in use (MenuPointerHand). Retail's windows are
// never marked, and draw as retail draws them.
void ClsMesSetChoiceRows(const ClsMes *mes, int first_row);

// The choice window's choices run on past it, above the first in view or below the last: it shows the
// page arrow below, and the page arrow turned over above the first choice, blinking as the page arrow does.
void ClsMesSetChoiceScroll(bool more_above, bool more_below);

// Messages laid over a window's own: the window looks a message up in file (a message file, as
// SetBuff takes one) first, then in its own. A port shop's lines over its shopkeeper's
// (custom_shop.cpp). One window at a time; a null mes or file ends it.
void ClsMesSetMessageOverlay(const ClsMes *mes, short *file);
