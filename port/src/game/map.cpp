#include "map.hpp"

namespace dc::game {

CMapObject *CMap::SetObject(int index, CFrameVu1 *frame, int category_no, int handle) {
    if (index >= 10 || index < 0) {
        return nullptr;
    }

    CMapObject *map_object = &object[index];
    map_object->handle = handle;
    map_object->category_no = category_no;
    map_object->SetFrame(frame, 0);
    return map_object;
}

CMapObject *CMap::SetObject(CFrameVu1 *frame, int category_no, int handle) {
    for (int index = 0; index < 10; index++) {
        if (object[index].handle < 0) {
            return SetObject(index, frame, category_no, handle);
        }
    }

    return nullptr;
}

CMapObject *CMap::GetObject(int index) {
    if (index >= 10 || index < 0) {
        return nullptr;
    }

    return &object[index];
}

void CMap::Draw() {
    for (CMapObject &map_object : object) {
        const int category_no = map_object.category_no;
        if (category_no < 0) {
            continue;
        }

        CMapCategoryAttr &attr = category[category_no];
        if (attr.lod[0] > 0.0f && draw_on != 0) {
            map_object.DrawLOD(attr.lod, attr.lowest, attr.highest, nullptr);
        } else {
            map_object.Draw();
        }
    }
}

void CMap::Initialize() {
    for (CMapCategoryAttr &attr : category) {
        attr.lod[0] = -1.0f;
        attr.lowest = 0;
        attr.highest = 3;
    }

    for (CMapObject &map_object : object) {
        map_object.Initialize();
    }

    draw_on = 1;
}

} // namespace dc::game
