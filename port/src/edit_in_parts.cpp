#include "common.h"

#include <cstdint>

#include "eparts_port.hpp"
#include "objanime.hpp"

// Retail's GetFuncPoint copies each interior part's function records with the host's EPARTS_FUNC_DATA
// stride and words; the disc's are the PS2's (eparts_port.hpp). parts carries the part's index, which
// LoadData turns into its frame.
PC_OVERRIDE int GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points) {
    const char      *definition = (char *) archive + archive[1];
    EPartsDiscHeader header = EPartsReadHeader(definition);
    int              i;

    for (i = 0; i < header.func_count; points++) {
        EPartsReadFunc(definition, i, points);
        points->parts = reinterpret_cast<CMapParts *>(static_cast<std::intptr_t>(parts_no));
        i++;
    }

    return header.func_count;
}
