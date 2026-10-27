#include "eparts_port.hpp"

#include <cstring>
#include <iterator>

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
