#include <cstring>
#include <memory>

#include "dataalloc.hpp"
#include "fireomni.hpp"
#include "map.hpp"
#include "mapobject.hpp"
#include "objanime.hpp"
#include "textureanime.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/rushmovi.hpp"
#include "title_port.hpp"
#include "water.hpp"

// The title overlay's objects use the host-sized layouts declared here rather than the title units'
// own. TitleOverlayConstruct rebuilds them the way the overlay loader does: zeroed, then constructed.

namespace {

template <class T>
void Construct(T &object) {
    std::memset(static_cast<void *>(&object), 0, sizeof(T));
    std::construct_at(&object);
}

} // namespace

extern CFireOmni     CFire;
extern OBJ_ANIME_SEQ OP_AnimeSeq[32];
extern CMapObject    OP_NornMapObj[76];
extern CMapObject    OP_NornMapObj2[87];

void TitleOverlayConstruct() {
    for (CMap *map : {&OP_GroundMap, &OP_BuildingMap, &OP_BuildingMap2}) {
        Construct(*map);
        // map.hpp's CMap constructor leaves initialisation to the caller.
        map->Initialize();
    }
    for (OBJ_ANIME_SEQ &sequence : OP_AnimeSeq) {
        Construct(sequence);
    }
    for (CMapObject &object : OP_NornMapObj) {
        Construct(object);
    }
    for (CMapObject &object : OP_NornMapObj2) {
        Construct(object);
    }
    Construct(CFire);
    Construct(CFire__4);
    Construct(Water);
    Construct(Water__2);
}
