#pragma once

// The Seda mod (docs/SEDA.md): Seda's cutscene character in Toan's place in the dungeon, from the
// pack dcdata builds out of the disc (tools/dcdata/dcdata.hpp, MakeSedaPack).

// --seda (or DC_SEDA=1), a test hook: Seda stands in for Toan from the start.
void SedaSetForced(bool forced);

// Whether Seda stands in for Toan now.
bool SedaActive();
