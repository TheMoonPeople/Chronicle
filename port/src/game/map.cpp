#include "map.hpp"

CMapObject *CMap::SetObject(int index, CFrameVu1 *frame, int category_no, int handle) {
    CMapObject *map_object;

    if (index >= 10 || index < 0) {
        return NULL;
    }

    object[index].handle = handle;
    object[index].category_no = category_no;
    map_object = &object[index];
    map_object->SetFrame(frame, 0);
    return map_object;
}

CMapObject *CMap::SetObject(CFrameVu1 *frame, int category_no, int handle) {
    int index;

    for (index = 0; index < 10; index++) {
        if (object[index].handle < 0) {
            break;
        }
    }

    if (index == 10) {
        return NULL;
    }

    return SetObject(index, frame, category_no, handle);
}

CMapObject *CMap::GetObject(int index) {
    if (index >= 10 || index < 0) {
        return NULL;
    }

    return &object[index];
}

void CMap::Draw() {
    int index;

    for (index = 0; index < 10; index++) {
        CMapObject       *map_object;
        int               category_no;
        CMapCategoryAttr *attr;

        map_object = &object[index];
        category_no = map_object->category_no;

        if (category_no < 0) {
            continue;
        }

        attr = &category[category_no];

        if (attr->lod[0] > 0.0f && draw_on != 0) {
            map_object->DrawLOD(attr->lod, attr->lowest, attr->highest, NULL);
        } else {
            map_object->Draw();
        }
    }
}

void CMap::Initialize() {
    int index;

    for (index = 0; index < 16; index++) {
        category[index].lod[0] = -1.0f;
        category[index].lowest = 0;
        category[index].highest = 3;
    }

    for (index = 0; index < 10; index++) {
        object[index].Initialize();
    }

    draw_on = 1;
}
