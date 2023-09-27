#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h> /* ntohs() */

#include "cdg.h"
#include "util.h"

static inline int cdg_color_to_rgb(uint16_t color) {
    /*
     * Each colorSpec value can be converted to RGB using the following diagram:
     * [---high byte---]   [---low byte----]
     *  7 6 5 4 3 2 1 0     7 6 5 4 3 2 1 0
     *  X X r r r r g g     X X g g b b b b
     */
    int r = ((color & 0x3C00) >> 10) * 16;
    int g = (((color & 0x300) >> 6) | ((color & 0x30) >> 4)) * 16;
    int b = (color & 0xF) * 16;

    return (r << 16) | (g << 8) | b;
}

// Closest, without going over - like The Price is Right :-)
static struct cdg_keyframe *cdg_reader_find_closest_keyframe(struct cdg_keyframe_list *list, cdg_ts_t ts) {
    // Binary search
    size_t low = 0;
    size_t high = list->count - 1;
    cdg_ts_t bestTs = -1;
    size_t bestIndex = (size_t) -1;

    while (low <= high) {
        size_t mid = (low + high) / 2;
        struct cdg_keyframe *keyframe = &list->keyframes[mid];

        if (keyframe->timestamp == ts) {
            return keyframe;
        } else if (keyframe->timestamp < ts) {
            if (bestIndex == (size_t) -1 || keyframe->timestamp > bestTs) {
                bestTs = keyframe->timestamp;
                bestIndex = mid;
            }

            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    assert(bestIndex >= 0 && bestIndex < list->count);

    return &list->keyframes[bestIndex];
}

static void cdg_reader_seek_to_keyframe(struct cdg_reader *reader, struct cdg_keyframe *keyframe) {
    reader->state.ts = keyframe->timestamp;
    reader->buffer_index = reader->state.ts * sizeof(struct subchannel_packet);

    // Load the color table
    memcpy(reader->state.color_table, keyframe->color_table, sizeof(reader->state.color_table));
    // Clear the screen
    memset(reader->state.framebuffer, keyframe->clear_color, (300 * 216) * sizeof(unsigned int));
}

struct cdg_reader *cdg_reader_new(void) {
    struct cdg_reader *reader = (struct cdg_reader *) malloc(sizeof(struct cdg_reader));

    CHECK_MEM(reader)

    memset(reader, 0, sizeof(struct cdg_reader));

    return reader;
}

void cdg_reader_free(struct cdg_reader *reader) {
    struct cdg_keyframe_list *list = &reader->keyframes;

    if (list->keyframes) {
        free(list->keyframes);
    }

    if (reader->buffer) {
        free(reader->buffer);
    }
}

void cdg_reader_reset(struct cdg_reader *reader) {
    reader->state.ts = 0;
    reader->buffer_index = 0;
    reader->eof = 0;
}

int cdg_reader_read_frame(struct cdg_reader *reader, struct subchannel_packet *outPkt) {
    size_t count = sizeof(struct subchannel_packet);

    if (reader->buffer_index + count > reader->buffer_size) {
        return 0;
    }

    memcpy(outPkt, reader->buffer + reader->buffer_index, count);
    reader->buffer_index += count;

    return 1;
}

// Returns: 1 if we need to update the framebuffer
int cdg_state_process_insn(struct cdg_state *state, struct subchannel_packet *pkt) {
    uint8_t code;
    struct cdg_insn *insn;

    state->ts++;

    // not a CDG packet
    if ((pkt->command & 0x3F /* 0b111111 */) != 9) {
        return 0;
    }

    code = pkt->instruction;
    insn = (struct cdg_insn *) pkt->data;

    switch (code) {
        // Load colors 0-7
        case CDG_INSN_LOAD_COLOR_TABLE_00:
        case CDG_INSN_LOAD_COLOR_TABLE_08: {
            struct cdg_insn_load_color_table *insn_load_color_table = (struct cdg_insn_load_color_table *) insn;
            size_t offset = code == CDG_INSN_LOAD_COLOR_TABLE_00 ? 0 : 8;

            for (int i = 0; i < 8; i++) {
                state->color_table[i + offset] = cdg_color_to_rgb(ntohs(insn_load_color_table->spec[i] & 0x3F3F));
            }

            return 1;
        }
        // Clear the screen
        case CDG_INSN_MEMORY_PRESET: {
            struct cdg_insn_memory_preset *insn_memory_preset = (struct cdg_insn_memory_preset *) insn;

            // The repeat code is incremented each time the same command is sent.
            // This is to ensure the screen is cleared in a potentially unreliable stream.
            // Since we're reading from a file, we can just check if the repeat code is 0 and only do this once.
            if (insn_memory_preset->repeat == 0) {
                memset(state->framebuffer, insn_memory_preset->color, (300 * 216) * sizeof(unsigned int));
            }

            return 1;
        }
        case CDG_INSN_BORDER_PRESET: {
            // printf("BORDER\n");
            // The border area is the area contained with a
            // rectangle defined by (0,0,300,216) minus the interior pixels which are contained
            // within a rectangle defined by (6,12,294,204).
            struct cdg_insn_border_preset *insn_border_preset = (struct cdg_insn_border_preset *) insn;

            for (int x = 0; x < 300; x++) {
                if (x > 6 && x < 294) {
                    continue;
                }

                for (int y = 0; y < 216; y++) {
                    if (y > 12 && y < 204) {
                        continue;
                    }

                    state->framebuffer[ARRAY_INDEX(x, y)] = insn_border_preset->color;
                }
            }
            return 1;
        }
        // Copy a block of pixels into the framebuffer
        case CDG_INSN_TILE_BLOCK:
        case CDG_INSN_TILE_BLOCK_XOR: {
            struct cdg_insn_tile_block *insn_tile_block = (struct cdg_insn_tile_block *) insn;
            int isXor = code == CDG_INSN_TILE_BLOCK_XOR;

            size_t startRow;
            size_t startCol;
            // Row and Column describe the position of the tile in tile coordinate space.  To
            // convert to pixels, multiply row by 12, and column by 6.
            startRow = (insn_tile_block->row & 0x1F) * 12;
            startCol = (insn_tile_block->column & 0x3F) * 6;

            // tilePixels[] contains the actual bit values for the tile, six pixels per byte.
            // The uppermost valid bit of each byte (0x20) contains the left-most pixel of each
            // scanline of the tile.
            // pretty sure it's 6 rows of 12 pixels, so 2 bytes per row

            for (int i = 0; i < 12; i++) {
                uint8_t tilePixels = insn_tile_block->pixels[i] & 0x3F;

                for (int j = 0; j < 6; j++) {
                    uint8_t pixel = (tilePixels >> (5 - j)) & 1;
                    uint8_t color = (pixel ? insn_tile_block->color_1 : insn_tile_block->color_0) & 0xF;

                    if (isXor) {
                        state->framebuffer[ARRAY_INDEX(startCol + j, startRow + i)] ^= color;
                    } else {
                        state->framebuffer[ARRAY_INDEX(startCol + j, startRow + i)] = color;
                    }
                }
            }

            return 1;
        }
        case CDG_INSN_SCROLL_PRESET:
        case CDG_INSN_SCROLL_COPY: {
            // TODO: implement - I haven't seen this in any CDG files yet
            printf("*** Unsupported scrolling instruction!\n");
//            struct cdg_insn_scroll *insn_scroll = (struct cdg_insn_scroll *) insn;
//            uint8_t color = insn_scroll->color;
//            uint8_t h_scroll = insn_scroll->h_scroll;
//            uint8_t v_scroll = insn_scroll->v_scroll;
        }
        break;
        default:
            printf("unexpected insn: %d\n", code);
    }

    return 0;
}

int cdg_reader_load_file(struct cdg_reader *reader, const char *path) {
    FILE *fp;

    fp = fopen(path, "r");

    if (!fp) {
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    reader->buffer_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    reader->buffer_index = 0;
    reader->buffer = (uint8_t *) malloc(reader->buffer_size);

    CHECK_MEM(reader->buffer)

    if (fread(reader->buffer, 1, reader->buffer_size, fp) != reader->buffer_size) {
        fclose(fp);

        free(reader->buffer);
        reader->buffer = NULL;
        reader->buffer_size = 0;

        return 0;
    }

    return 1;
}

void cdg_reader_build_keyframe_list(struct cdg_reader *reader) {
    struct subchannel_packet insn;
    struct cdg_keyframe *keyframe;

    cdg_ts_t ts = 0;
    int colorTable[16] = { 0 };

    struct cdg_keyframe_list *list = &reader->keyframes;

    list->keyframes = NULL;
    list->count = 0;

    cdg_reader_reset(reader);

    while (cdg_reader_read_frame(reader, &insn)) {
        ts++;

        // not a CDG packet
        if ((insn.command & 0x3F /* 0b111111 */) != 9) {
            continue;
        }

        if (insn.instruction == CDG_INSN_LOAD_COLOR_TABLE_00) {
            for (int i = 0; i < 8; i++) {
                colorTable[i] = cdg_color_to_rgb(ntohs(((struct cdg_insn_load_color_table *) insn.data)->spec[i]));
            }
        } else if (insn.instruction == CDG_INSN_LOAD_COLOR_TABLE_08) {
            for (int i = 0; i < 8; i++) {
                colorTable[i + 8] = cdg_color_to_rgb(ntohs(((struct cdg_insn_load_color_table *) insn.data)->spec[i]));
            }
        } else if (insn.instruction == CDG_INSN_MEMORY_PRESET) {
            if (((struct cdg_insn_memory_preset *) insn.data)->repeat != 0) {
                continue;
            }

            list->keyframes = (struct cdg_keyframe *) realloc(
                    list->keyframes,
                    sizeof(struct cdg_keyframe) * (list->count + 1)
            );

            CHECK_MEM(list->keyframes)

            keyframe = &list->keyframes[list->count];

            keyframe->timestamp = ts + 1;
            memcpy(keyframe->color_table, colorTable, sizeof(colorTable));
            keyframe->clear_color = ((struct cdg_insn_memory_preset *) insn.data)->color;

            list->count++;
        }
    }

    cdg_reader_reset(reader);
}

int cdg_reader_seek(struct cdg_reader *reader, cdg_ts_t ts) {
    struct cdg_keyframe *keyframe;
    struct subchannel_packet pkt;
    int needsUpdate;

    if (ts > reader->state.ts) {
        // Seeking forward: just go straight to the timestamp we want.
        goto seek_forward;
    }

    // Seeking backward: find the closest keyframe and go there first.
    keyframe = cdg_reader_find_closest_keyframe(&reader->keyframes, ts);
    assert(keyframe != NULL);
    cdg_reader_seek_to_keyframe(reader, keyframe);

    // ...and then seek forward to the timestamp we want.
seek_forward:
    needsUpdate = 0;
    while (reader->state.ts < ts) {
        if (!cdg_reader_read_frame(reader, &pkt)) {
            // End of CDG stream
            printf("cdg_reader_seek(): end of stream\n");
            reader->eof = 1;
            break;
        }

        // Intentionally using |= here so that cdg_state_process_insn() is always called
        needsUpdate |= cdg_state_process_insn(&reader->state, &pkt);
    }

    return needsUpdate;
}
