#include "textureanime.hpp"

#include "texture.hpp"
#include "texture_port.hpp"

namespace {

void Move(const CTextureTexAnime &from, int x, int y, int width, int height, const CTextureTexAnime &to, int to_x,
          int to_y) {
    PortMoveImage(*(const sceGsTex0 *) &from.tex0, x, y, width, height, *(const sceGsTex0 *) &to.tex0, to_x, to_y);
}

} // namespace

// Retail's TexAnime with each MGMoveImage done on the renderer and the TEXFLUSH packets gone.
PC_OVERRIDE void CTextureAnime::TexAnime(int texture_block) {
    for (int i = 0; i < 24; i++) {
        if (enabled[i] != 0 && current[i] != NULL && current[i]->linked_group >= 0) {
            Enable(current[i]->linked_group);
        }
    }

    for (int group = 0; group < 24; group++) {
        if (enabled[group] == 0 || current[group] == NULL || current[group]->first_texture.block != texture_block ||
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
                    const sceGsTex0 &first = *(const sceGsTex0 *) &record->first_texture.tex0;
                    const sceGsTex0 &second = *(const sceGsTex0 *) &record->second_texture.tex0;
                    PortMoveImage(first.CBP, first.CPSM, 0, 0, 16, 16, second.CBP, second.CPSM, 0, 0);
                }
            }

            if (record->kind == 1) {
                int scroll_x = (int) record->scroll_x;
                int scroll_y = (int) record->scroll_y;
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

            if (record->duration != 0 || record->next == NULL) {
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

            if (current[group] == NULL) {
                current[group] = first[group];
            }
        }
    }
}
