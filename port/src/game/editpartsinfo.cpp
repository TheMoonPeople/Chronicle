#include "editpartsinfo.hpp"

#include "editatra.hpp"
#include "editloop3.hpp"
#include "savedata.hpp"

#include <cstring>
#include <iterator>

namespace dc::game {

EDITPARTS_INFO *CEditPartsInfo::GetPartsInfo(int index) {
    if (index < 0 || index >= 24) {
        return nullptr;
    }

    return &parts[index];
}

int CEditPartsInfo::CheckComplete(int index) {
    EDITPARTS_INFO *info = GetPartsInfo(index);

    if (info == nullptr) {
        return 0;
    }

    for (const EDITPARTS_ELEMENT &element : info->elements) {
        // A negative identifier is an unused slot, so it asks for nothing.
        if (element.id >= 0 && element.enabled == 0) {
            return 0;
        }
    }

    return 1;
}

void CEditPartsInfo::SetCompEvent(int index, int completed) {
    EDITPARTS_INFO *info = GetPartsInfo(index);

    if (info == nullptr) {
        return;
    }

    if (completed != 0) {
        info->completion_flags |= 1;
    } else {
        info->completion_flags &= ~1;
    }
}

int CEditPartsInfo::GetCompEvent(int index) {
    EDITPARTS_INFO *info = GetPartsInfo(index);

    if (info == nullptr) {
        return 0;
    }

    return static_cast<int>((info->completion_flags & 1) != 0);
}

int CEditPartsInfo::GetRequest(int index) {
    if (index < 0 || index >= 24) {
        return 0;
    }

    return request[index];
}

int CEditPartsInfo::GetNextPartsNum(int index) {
    int remaining = 0;

    for (int i = index + 1; i < 24; i++) {
        EDITPARTS_INFO *info = GetPartsInfo(i);

        if (info == nullptr) {
            break;
        }

        if (CheckPartsInfo(info) != 0) {
            remaining++;
        }
    }

    return remaining;
}

int CEditPartsInfo::GetNextParts(int index) {
    for (;;) {
        index++;
        EDITPARTS_INFO *info = GetPartsInfo(index);

        if (info == nullptr) {
            return -1;
        }

        if (CheckPartsInfo(info) != 0) {
            return index;
        }
    }
}

void CEditPartsInfo::Clear() {
    for (EDITPARTS_INFO &part : parts) {
        part.placed = 0;
    }
}

void CEditPartsInfo::Save(int georama_no, CSaveData *save_data) {
    SV_GEORAMA_DATA *georama = save_data->GetGrdData(georama_no);

    if (georama == nullptr) {
        return;
    }

    georama->request_count = parts_max;

    for (int plot = 0; plot < 24; plot++) {
        georama->request_complete[plot] = request[plot];
        SV_EDIT_PARTS_INFO *saved_part = save_data->GetEditPartsInfo(georama_no, plot);

        if (saved_part == nullptr) {
            continue;
        }

        saved_part->obtained = parts[plot].obtained;
        saved_part->completion_flags = parts[plot].completion_flags;
        saved_part->placed = parts[plot].placed;
        saved_part->stock = parts[plot].stock;

        for (int element = 0; element < 6; element++) {
            saved_part->element_enabled[element] = parts[plot].elements[element].enabled;
        }
    }
}

void CEditPartsInfo::Load(int georama_no, CSaveData *save_data, int load_requests) {
    if (georama_no < 0 || georama_no >= 6) {
        return;
    }

    Initialize(georama_no);
    SV_GEORAMA_DATA *georama = save_data->GetGrdData(georama_no);

    if (georama == nullptr) {
        return;
    }

    if (load_requests != 0) {
        parts_max = georama->request_count;
    }

    for (int plot = 0; plot < 24; plot++) {
        SV_EDIT_PARTS_INFO *saved_part = save_data->GetEditPartsInfo(georama_no, plot);

        if (saved_part == nullptr) {
            continue;
        }

        parts[plot].obtained = saved_part->obtained;
        parts[plot].completion_flags = saved_part->completion_flags;

        if (load_requests != 0) {
            parts[plot].placed = saved_part->placed;
            request[plot] = georama->request_complete[plot];
        }

        if (saved_part->stock > 0) {
            parts[plot].stock = saved_part->stock;
        }

        for (int element = 0; element < 6; element++) {
            parts[plot].elements[element].enabled = saved_part->element_enabled[element];
        }
    }
}

void CEditPartsInfo::Initialize(int georama_no) {
    EDIT_PARTS_ATRA empty_part{};

    for (EDIT_CHIP_ATTACH_DATA &element : empty_part.elements) {
        element.required_element = -1;
    }

    unk_1624 = 0;

    for (int plot = 0; plot < 24; plot++) {
        const EDIT_PARTS_ATRA *source = nullptr;

        if (georama_no < 0 || georama_no >= 6) {
            source = &empty_part;
        } else {
            source = GetEditAtraPartsData(georama_no, plot);
        }

        EDITPARTS_INFO &part = parts[plot];
        part.parts_no = plot;
        part.obtained = 0;
        part.completion_flags = 0;
        part.stock = source->max;
        part.placed = 0;
        part.tex_no = source->tex_no;
        part.kind = source->kind;

        for (int element = 0; element < 6; element++) {
            const EDIT_CHIP_ATTACH_DATA &source_element = source->elements[element];
            EDITPARTS_ELEMENT           &target = part.elements[element];

            target.id = source_element.id;
            target.required_element = source_element.required_element;
            target.enabled = 0;

            for (int name_no = 0; name_no < 4; name_no++) {
                target.names[name_no] = nullptr;

                if (source_element.names[name_no] != nullptr && source_element.names[name_no][0] != '\0') {
                    target.names[name_no] = source_element.names[name_no];
                }
            }

            target.npc_no = source_element.npc_no;
        }
    }
}

void CEditPartsInfo::Initialize(int index, EPARTS_INFO_HEADER *header) {
    EDITPARTS_INFO &part = parts[index];
    part.parts_no = index;
    part.obtained = 0;
    part.completion_flags = 0;
    part.placed = 0;
    part.width = header->width;
    part.height = header->height;
    part.header = header;
}

namespace {

template <typename T>
T *Carve(CDataAlloc2<1> *arena, std::size_t bytes) {
    return reinterpret_cast<T *>(arena->Alloc(static_cast<int>((bytes >> 4) + 1)));
}

} // namespace

EPartsDiscHeader EPartsReadHeader(const void *record) {
    EPartsDiscHeader header{};
    std::memcpy(&header, record, sizeof(header));
    return header;
}

void EPartsReadFunc(const void *record, int index, EPARTS_FUNC_DATA *out) {
    EPartsDiscHeader header = EPartsReadHeader(record);
    EPartsDiscFunc   disc{};
    std::memcpy(&disc,
                static_cast<const char *>(record) + header.func +
                    static_cast<std::size_t>(index) * sizeof(EPartsDiscFunc),
                sizeof(disc));

    std::memset(out, 0, sizeof(*out));
    std::memcpy(out->unk_00, disc.unk_00, sizeof(out->unk_00));
    out->kind = disc.kind;
    out->parts = reinterpret_cast<CMapParts *>(static_cast<std::uintptr_t>(disc.parts));
    out->start_time = disc.start_time;
    out->end_time = disc.end_time;
    out->link_id = disc.link_id;
    out->completion_flag = disc.completion_flag;
    std::memcpy(out->unk_28, disc.unk_28, sizeof(out->unk_28));
    std::memcpy(out->frame_name, disc.frame_name, sizeof(out->frame_name));
    std::memcpy(out->position, disc.position, sizeof(out->position));
    std::memcpy(out->rotation, disc.rotation, sizeof(out->rotation));
    std::memcpy(out->parameters, disc.parameters, sizeof(out->parameters));
    std::memcpy(out->values, disc.values, sizeof(out->values));
    std::memcpy(out->matrix, disc.matrix, sizeof(out->matrix));
}

EPARTS_INFO_HEADER *EPartsLoad(const void *record, CDataAlloc2<1> *arena) {
    EPartsDiscHeader source = EPartsReadHeader(record);

    auto *header = Carve<EPARTS_INFO_HEADER>(arena, sizeof(EPARTS_INFO_HEADER));
    char *blob = Carve<char>(arena, static_cast<std::size_t>(source.data_size));
    std::memcpy(blob, record, static_cast<std::size_t>(source.data_size));

    std::memset(header, 0, sizeof(*header));
    header->header_size = source.header_size;
    header->data_size = source.data_size;
    header->width = source.width;
    header->height = source.height;
    header->kind = source.kind;
    std::memcpy(header->unk_14, source.unk_14, sizeof(header->unk_14));
    std::memcpy(header->position, source.position, sizeof(header->position));
    std::memcpy(header->rotation, source.rotation, sizeof(header->rotation));
    header->cell = reinterpret_cast<u8 *>(&blob[source.cell]);

    for (std::size_t element = 0; element < std::size(source.element_id); element++) {
        header->element_id[element] = source.element_id[element];
        header->element_name[element] =
            source.element_name[element] != 0 ? &blob[source.element_name[element]] : nullptr;
    }

    header->func_count = source.func_count;
    auto count = static_cast<std::size_t>(source.func_count > 0 ? source.func_count : 0);
    header->func = Carve<EPARTS_FUNC_DATA>(arena, sizeof(EPARTS_FUNC_DATA) * count);

    for (int func_index = 0; func_index < source.func_count; func_index++) {
        EPartsReadFunc(record, func_index, &header->func[func_index]);
    }

    return header;
}

} // namespace dc::game
