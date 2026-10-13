#pragma once

// Buttons CGamePad::Down reports as not newly pressed until the mask changes, whatever pad 1 does.
// 0 clears it.
void PadHoldDown(int mask);
