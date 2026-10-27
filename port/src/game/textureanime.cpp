#include "textureanime.hpp"

#include <array>

#include "render/texture_registry.hpp"
#include "scriptinterpreter.hpp"
#include "texture.hpp"

namespace dc::game {

using namespace dc::render;

s32 CTextureAnime::stop_anime;

namespace {

CTextureAnime *pTexAnime;
int            now_group;

/**
 * Enables or disables the texture-animation group selected by configuration parsing.
 */
void CommandTEX_ANIME(void **arguments) {
    now_group = *static_cast<int *>(arguments[0]);

    if (*static_cast<int *>(arguments[1]) != 0) {
        pTexAnime->Enable(now_group);
        return;
    }

    pTexAnime->Disable(now_group);
}

/**
 * Builds and appends a two-texture animation record from configuration arguments.
 */
void CommandTEX_ANIME_DATA(void **arguments) {
    CTexAnimeData record;
    record.group = static_cast<s16>(now_group);
    record.kind = 0;
    char *first_name = static_cast<char *>(arguments[0]);
    record.source_x = *static_cast<s16 *>(arguments[1]);
    record.source_y = *static_cast<s16 *>(arguments[2]);
    record.source_width = *static_cast<s16 *>(arguments[3]);
    record.source_height = *static_cast<s16 *>(arguments[4]);
    char *second_name = static_cast<char *>(arguments[5]);
    record.dest_x = *static_cast<s16 *>(arguments[6]);
    record.dest_y = *static_cast<s16 *>(arguments[7]);
    record.duration = *static_cast<s16 *>(arguments[8]);

    if (*static_cast<int *>(arguments[9]) != 0) {
        record.duration = -1;
    }

    CTexture *first_texture = TexManager.GetTexture(first_name, -1);
    CTexture *second_texture = TexManager.GetTexture(second_name, -1);
    record.first_texture = CTextureTexAnime{};
    record.first_texture.block = -1;
    record.second_texture = CTextureTexAnime{};
    record.second_texture.block = -1;

    if (first_texture != nullptr && second_texture != nullptr) {
        record.first_texture.Copy(first_texture);
        record.second_texture.Copy(second_texture);
        pTexAnime->EnterTexAnime(&record);
    }
}

/**
 * Builds and appends a two-texture animation record with an explicit auxiliary index.
 */
void CommandTEX_ANIME_DATA2(void **arguments) {
    CTexAnimeData record;
    record.group = static_cast<s16>(now_group);
    record.kind = 0;
    char *first_name = static_cast<char *>(arguments[0]);
    record.source_x = *static_cast<s16 *>(arguments[1]);
    record.source_y = *static_cast<s16 *>(arguments[2]);
    record.source_width = *static_cast<s16 *>(arguments[3]);
    record.source_height = *static_cast<s16 *>(arguments[4]);
    char *second_name = static_cast<char *>(arguments[5]);
    record.dest_x = *static_cast<s16 *>(arguments[6]);
    record.dest_y = *static_cast<s16 *>(arguments[7]);
    record.duration = *static_cast<s16 *>(arguments[8]);
    int hold = *static_cast<int *>(arguments[9]);
    record.linked_group = *static_cast<s16 *>(arguments[10]);

    if (hold != 0) {
        record.duration = -1;
    }

    CTexture *first_texture = TexManager.GetTexture(first_name, -1);
    CTexture *second_texture = TexManager.GetTexture(second_name, -1);

    if (first_texture != nullptr && second_texture != nullptr) {
        record.first_texture.Copy(first_texture);
        record.second_texture.Copy(second_texture);
        pTexAnime->EnterTexAnime(&record);
    }
}

/**
 * Builds and appends a scrolling texture-animation record from configuration arguments.
 */
void CommandTEX_SCROLL_DATA(void **arguments) {
    CTexAnimeData record;
    record.group = static_cast<s16>(now_group);
    record.kind = 1;
    char *first_name = static_cast<char *>(arguments[0]);
    record.source_x = *static_cast<s16 *>(arguments[1]);
    record.source_y = *static_cast<s16 *>(arguments[2]);
    record.source_width = *static_cast<s16 *>(arguments[3]);
    record.source_height = *static_cast<s16 *>(arguments[4]);
    char *second_name = static_cast<char *>(arguments[5]);
    record.dest_x = *static_cast<s16 *>(arguments[6]);
    record.dest_y = *static_cast<s16 *>(arguments[7]);
    record.scroll_x_step = *static_cast<float *>(arguments[8]);
    record.scroll_y_step = *static_cast<float *>(arguments[9]);
    record.duration = *static_cast<s16 *>(arguments[10]);

    if (*static_cast<int *>(arguments[11]) != 0) {
        record.duration = -1;
    }

    CTexture *first_texture = TexManager.GetTexture(first_name, -1);
    CTexture *second_texture = TexManager.GetTexture(second_name, -1);

    if (first_texture != nullptr && second_texture != nullptr) {
        record.first_texture.Copy(first_texture);
        record.second_texture.Copy(second_texture);
        pTexAnime->EnterTexAnime(&record);
    }
}

/**
 * Ends a texture animation command; nothing remains to be done.
 */
void CommandTEX_ANIME_END(void ** /*arguments*/) {
}

/** The tags LoadCFGFile recognises, each with its argument kinds. */
std::array<TAG_PARAM, 5> Command = {
    {
     {.name = "TEX_ANIME", .argument_types = {1, 1, -1}},
     {.name = "TEX_ANIME_DATA", .argument_types = {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, -1}},
     {.name = "TEX_ANIME_DATA2", .argument_types = {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, -1}},
     {.name = "TEX_SCROLL_DATA", .argument_types = {0, 1, 1, 1, 1, 0, 1, 1, 2, 2, 1, 1, -1}},
     {.name = "TEX_ANIME_END", .argument_types = {-1}},
     }
};

/** The handler LoadCFGFile calls for each of Command's tags. */
constexpr std::array<void (*)(void **arguments), 5> CommandExe = {
    CommandTEX_ANIME,
    CommandTEX_ANIME_DATA,
    CommandTEX_ANIME_DATA2,
    CommandTEX_SCROLL_DATA,
    CommandTEX_ANIME_END,
};

} // namespace

void CTextureTexAnime::Copy(CTexture *texture) {
    if (texture != nullptr) {
        block = texture->block;
        width = texture->width;
        height = texture->height;
        bpp = texture->bpp;
        tex0 = texture->tex0;
        tex1 = texture->tex1;
    }
}

CTexAnimeData::CTexAnimeData() {
    Initialize();
}

void CTexAnimeData::Initialize() {
    kind = -1;
    group = 0;
    duration = 0;
    first_texture = CTextureTexAnime{};
    first_texture.block = -1;
    second_texture = CTextureTexAnime{};
    second_texture.block = -1;
    source_height = 0;
    source_width = 0;
    source_y = 0;
    source_x = 0;
    dest_y = 0;
    dest_x = 0;
    scroll_y = 0.0f;
    scroll_x = 0.0f;
    scroll_y_step = 0.0f;
    scroll_x_step = 0.0f;
    next = nullptr;
    linked_group = -1;
}

namespace {

void Move(const CTextureTexAnime &from, int x, int y, int width, int height, const CTextureTexAnime &to, int to_x,
          int to_y) {
    PortMoveImage(*reinterpret_cast<const sceGsTex0 *>(&from.tex0), x, y, width, height,
                  *reinterpret_cast<const sceGsTex0 *>(&to.tex0), to_x, to_y);
}

} // namespace

void CTextureAnime::TexAnime(int texture_block) {
    for (int i = 0; i < 24; i++) {
        if (enabled[i] != 0 && current[i] != nullptr && current[i]->linked_group >= 0) {
            Enable(current[i]->linked_group);
        }
    }

    for (int group = 0; group < 24; group++) {
        if (enabled[group] == 0 || current[group] == nullptr || current[group]->first_texture.block != texture_block ||
            current[group]->second_texture.block != texture_block) {
            continue;
        }

        CTexAnimeData *record = current[group];

        for (;;) {
            if (record->kind == 0) {
                Move(record->first_texture, record->source_x, record->source_y, record->source_width,
                     record->source_height, record->second_texture, record->dest_x, record->dest_y);

                if (record->source_width == record->first_texture.width &&
                    record->source_height == record->first_texture.height && record->first_texture.bpp == 1) {
                    const sceGsTex0 &first = *reinterpret_cast<const sceGsTex0 *>(&record->first_texture.tex0);
                    const sceGsTex0 &second = *reinterpret_cast<const sceGsTex0 *>(&record->second_texture.tex0);
                    PortMoveImage(first.CBP, first.CPSM, 0, 0, 16, 16, second.CBP, second.CPSM, 0, 0);
                }
            }

            if (record->kind == 1) {
                int scroll_x = static_cast<int>(record->scroll_x);
                int scroll_y = static_cast<int>(record->scroll_y);
                int width = record->source_width - scroll_x;
                int height = record->source_height - scroll_y;

                if (width > 0 && height > 0) {
                    Move(record->first_texture, record->source_x + scroll_x, record->source_y + scroll_y, width,
                         height, record->second_texture, record->dest_x, record->dest_y);
                }

                if (width > 0 && scroll_y > 0) {
                    Move(record->first_texture, record->source_x + scroll_x, record->source_y, width, scroll_y,
                         record->second_texture, record->dest_x, record->dest_y + record->source_height - scroll_y);
                }

                if (scroll_x > 0 && height > 0) {
                    Move(record->first_texture, record->source_x, record->source_y + scroll_y, scroll_x, height,
                         record->second_texture, record->dest_x + record->source_width - scroll_x, record->dest_y);
                }

                // Retail places this corner from source_x, unlike the other three pieces.
                if (scroll_x > 0 && scroll_y > 0) {
                    Move(record->first_texture, record->source_x, record->source_y, scroll_x, scroll_y,
                         record->second_texture, record->source_x + record->source_width - scroll_x,
                         record->dest_y + record->source_height - scroll_y);
                }
            }

            if (record->kind == 1 && CTextureAnime::stop_anime == 0) {
                if (record->scroll_x_step != 0.0f) {
                    float scroll = record->scroll_x + record->scroll_x_step;
                    record->scroll_x = scroll;

                    if (scroll >= record->source_width) {
                        record->scroll_x = scroll - record->source_width;
                    }

                    if (record->scroll_x < 0.0f) {
                        record->scroll_x = record->source_width + record->scroll_x;
                    }
                }

                if (record->scroll_y_step != 0.0f) {
                    float scroll = record->scroll_y + record->scroll_y_step;
                    record->scroll_y = scroll;

                    if (scroll >= record->source_height) {
                        record->scroll_y = scroll - record->source_height;
                    }

                    if (record->scroll_y < 0.0f) {
                        record->scroll_y = record->source_height + record->scroll_y;
                    }
                }
            }

            if (record->duration != 0 || record->next == nullptr) {
                break;
            }

            record = record->next;
        }

        if (CTextureAnime::stop_anime == 0) {
            frame[group]++;
        }

        if (record->duration < 0) {
            frame[group] = 0;
        } else if (frame[group] > record->duration) {
            frame[group] = 0;
            current[group] = current[group]->next;

            if (current[group] == nullptr) {
                current[group] = first[group];
            }
        }
    }
}

void CTextureAnime::Initialize(CTexAnimeData *records, int count) {
    data = records;
    data_count = count;

    for (int group = 0; group < kGroupCount; group++) {
        first[group] = nullptr;
        current[group] = nullptr;
        last[group] = nullptr;
        frame[group] = 0;
        enabled[group] = 0;
    }
}

CTextureAnime::CTextureAnime(CTexAnimeData *records, int count) {
    Initialize(records, count);
}

CTexAnimeData *CTextureAnime::NewTexAnimeData() {
    if (data == nullptr) {
        return nullptr;
    }

    for (int index = 0; index < data_count; index++) {
        if (data[index].kind == -1) {
            return &data[index];
        }
    }

    return nullptr;
}

CTexAnimeData *CTextureAnime::NewTexAnimeGroupData(int group) {
    if (group < 0 || group >= kGroupCount) {
        return nullptr;
    }

    CTexAnimeData *record = NewTexAnimeData();

    if (record == nullptr) {
        return nullptr;
    }

    record->Initialize();

    if (last[group] != nullptr) {
        last[group]->next = record;
    }

    last[group] = record;
    record->next = nullptr;

    if (current[group] == nullptr) {
        current[group] = record;
    }

    if (first[group] == nullptr) {
        first[group] = record;
    }

    return record;
}

int CTextureAnime::EnterTexAnime(CTexAnimeData *source) {
    CTexAnimeData *record = NewTexAnimeGroupData(source->group);

    if (record == nullptr) {
        return 0;
    }

    record->kind = source->kind;
    record->group = source->group;
    record->duration = source->duration;
    record->linked_group = source->linked_group;
    record->first_texture = source->first_texture;
    record->second_texture = source->second_texture;
    record->source_x = source->source_x;
    record->source_y = record->first_texture.height - source->source_y - source->source_height;
    record->source_width = source->source_width;
    record->source_height = source->source_height;
    record->dest_x = source->dest_x;
    record->dest_y = record->second_texture.height - source->dest_y - source->source_height;
    record->scroll_x_step = source->scroll_x_step;
    record->scroll_y_step = source->scroll_y_step;
    return 1;
}

void CTextureAnime::DisableAll() {
    for (int group = 0; group < kGroupCount; group++) {
        Disable(group);
    }
}

void CTextureAnime::Enable(int group) {
    if (group < 0 || group >= kGroupCount) {
        return;
    }

    enabled[group] = 1;
}

void CTextureAnime::Disable(int group) {
    if (group < 0 || group >= kGroupCount) {
        return;
    }

    enabled[group] = 0;
    current[group] = first[group];
    frame[group] = 0;
}

void CTextureAnime::LoadCFGFile(char *script, int script_size) {
    pTexAnime = this;
    now_group = 0;
    DisableAll();

    CScriptInterpreter interpreter;
    interpreter.SetScript(script, script_size);
    interpreter.SetTAG(Command.data(), static_cast<int>(Command.size()));
    for (int command = interpreter.GetNextTAG(); command >= 0; command = interpreter.GetNextTAG()) {
        CommandExe[command](interpreter.arguments);
    }
}

} // namespace dc::game
