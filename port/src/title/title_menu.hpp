#pragma once

#include <string>
#include <utility>
#include <vector>

// The title screen's menu: retail's three rows (start, continue, option) and the port's Exit, drawn
// from the font through the UI framework rather than from retail's painted rows, which have no Exit.
// Its rows keep CCursol's numbering: 0 to 2 are retail's, so TitleLoop's own handling of them is
// unchanged, and Exit comes after them. Exit asks before closing the game.
namespace TitleMenu {

// Exit's row. It shares TITLE_MENU_ATTRACT's number, which retail only sets on the way to the attract
// movie; Exit never reaches TitleLoop's fade-out, so the two never meet.
constexpr int kExitRow = 3;
constexpr int kRowCount = 4;

// Where TitleLoop's hand points at a row (CCursol.Set), on the screen.
float HandY(int row);

// The row under a point on the screen, or -1.
int RowAt(float x, float y);

// Draws the rows, the selected one at selected_alpha and the rest at other_alpha, as retail draws its
// painted rows.
void Draw(int selected, int selected_alpha, int other_alpha);

// The exit confirmation.
bool ConfirmOpen();
void OpenConfirm();

// One tick of the confirmation: the pad, and the pointer's hovered button (-1 for none), click
// and right click. Choosing yes closes the game.
void StepConfirm(int pointer_hover, bool pointer_click, bool pointer_back);

// The confirmation's button under a logical point, or -1.
int ConfirmHit(float x, float y);

void DrawConfirm(int alpha);

// Translation keys and English defaults.
std::vector<std::pair<std::string, std::string>> Strings();

} // namespace TitleMenu
