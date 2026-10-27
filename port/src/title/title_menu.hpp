#pragma once

#include <string>
#include <utility>
#include <vector>

/**
 * The title screen's menu: retail's three rows (start, continue, option) and the port's Exit, drawn
 * from the font through the UI framework rather than from retail's painted rows, which have no Exit.
 * Rows keep CCursol's numbering: 0 to 2 are retail's, so TitleLoop's handling of them is unchanged,
 * and Exit comes after them. Exit asks before closing the game.
 */
namespace dc::title::menu {

/**
 * Exit's row. It shares TITLE_MENU_ATTRACT's number, which retail only sets on the way to the attract
 * movie; Exit never reaches TitleLoop's fade-out, so the two never meet.
 */
constexpr int kExitRow = 3;

/**
 * Number of menu rows, Exit included.
 */
constexpr int kRowCount = 4;

/**
 * Screen y at which TitleLoop's hand points at a row (CCursol.Set).
 */
float HandY(int row);

/**
 * The row under a screen point, or -1 when the point is outside every row.
 */
int RowAt(float x, float y);

/**
 * Draws the rows, the selected one at selected_alpha and the rest at other_alpha,
 * as retail draws its painted rows.
 */
void Draw(int selected, int selected_alpha, int other_alpha);

/**
 * Whether the exit confirmation is showing.
 */
bool ConfirmOpen();

/**
 * Opens the exit confirmation with No selected and plays the confirm sound.
 */
void OpenConfirm();

/**
 * One tick of the confirmation: the pad, and the pointer's hovered button (-1 for none), click
 * and right click. Choosing yes closes the game.
 */
void StepConfirm(int pointer_hover, bool pointer_click, bool pointer_back);

/**
 * The confirmation's button under a logical point, or -1.
 */
int ConfirmHit(float x, float y);

/**
 * Draws the exit confirmation at alpha when it is open.
 */
void DrawConfirm(int alpha);

/**
 * The menu's translation keys and their English defaults.
 */
std::vector<std::pair<std::string, std::string>> Strings();

} // namespace dc::title::menu
