#pragma once

// A fix to retail's species table: lets the regular monsters MonstorTable leaves unable to drop
// anything drop as the rest do. Idempotent; RunGame calls it once at start.
void FixMonsterDrops();
