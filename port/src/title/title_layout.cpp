#include "title_port.hpp"

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
#include "water.hpp"

// The title units declare these with PS2 layouts of their own (op_a.cpp's 144-byte OBJ_ANIME_SEQ,
// rushmovi.cpp's 816-byte CWater and so on). The port's definitions are PC_OVERRIDE, so everything
// the title overlay reads goes through these host-sized ones. TitleOverlayConstruct builds them
// again as retail's overlay loader did: the .bss zeroed, then the constructors.

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
        // op_a.cpp's CMap constructor initialises the map; map.hpp's leaves that to the caller.
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
