#include <stdio.h>
#include <string.h>
#include <arpa/inet.h> /* ntohs() */

#include "cdg.h"

int read_subchannel_packet(FILE *fp, struct subchannel_packet *pkt) {
    return fread(pkt, 1, sizeof(struct subchannel_packet), fp) == sizeof(struct subchannel_packet);
}

void print_cdg_insn_memory_preset(struct cdg_insn_memory_preset *insn) {
    printf("MEMORY PRESET { color = %02X, repeat = %02X }\n", insn->color, insn->repeat);
}

void print_cdg_insn_border_preset(struct cdg_insn_border_preset *insn) {
    printf("BORDER PRESET { color = %02X }\n", insn->color);
}

void print_cdg_insn_tile_block(const char *tag, struct cdg_insn_tile_block *insn) {
    printf("TILE BLOCK %s { color_0 = %02X, color_1 = %02X, row = %02X, column = %02X, pixels = ", tag, insn->color_0, insn->color_1, insn->row, insn->column);

    for (int i = 0; i < 12; i++) {
        printf("%02X ", insn->pixels[i]);
    }

    printf("}\n");
}

void print_cdg_insn_scroll_preset(struct cdg_insn_scroll *insn) {
    printf("SCROLL PRESET { color = %02X, h_scroll = %02X, v_scroll = %02X }\n", insn->color, insn->h_scroll, insn->v_scroll);
}

void print_cdg_insn_scroll_copy(struct cdg_insn_scroll *insn) {
    printf("SCROLL COPY { color = %02X, h_scroll = %02X, v_scroll = %02X }\n", insn->color, insn->h_scroll, insn->v_scroll);
}

void print_cdg_insn_define_transparent(struct cdg_insn_define_transparent *insn) {
    printf("DEFINE TRANSPARENT\n");
}

void print_cdg_insn_load_color_table(const char *tag, struct cdg_insn_load_color_table *insn) {
    printf("LOAD COLOR TABLE %s { spec = ", tag);

    for (int i = 0; i < 8; i++) {
        printf("%04X ", insn->spec[i]);
    }

    printf("}\n");
}

void process_cdg_insn(struct subchannel_packet *pkt) {
    switch (pkt->instruction) {
        case CDG_INSN_MEMORY_PRESET:
            print_cdg_insn_memory_preset((struct cdg_insn_memory_preset *) pkt->data);
            break;
        case CDG_INSN_BORDER_PRESET:
            print_cdg_insn_border_preset((struct cdg_insn_border_preset *) pkt->data);
            break;
        case CDG_INSN_TILE_BLOCK:
            print_cdg_insn_tile_block("NORMAL", (struct cdg_insn_tile_block *) pkt->data);
            break;
        case CDG_INSN_SCROLL_PRESET:
            print_cdg_insn_scroll_preset((struct cdg_insn_scroll *) pkt->data);
            break;
        case CDG_INSN_SCROLL_COPY:
            print_cdg_insn_scroll_copy((struct cdg_insn_scroll *) pkt->data);
            break;
        case CDG_INSN_DEF_TRANSPARENT:
            print_cdg_insn_define_transparent((struct cdg_insn_define_transparent *) pkt->data);
            break;
        case CDG_INSN_LOAD_COLOR_TABLE_00:
            print_cdg_insn_load_color_table("0-7", (struct cdg_insn_load_color_table *) pkt->data);
            break;
        case CDG_INSN_LOAD_COLOR_TABLE_08:
            print_cdg_insn_load_color_table("8-15", (struct cdg_insn_load_color_table *) pkt->data);
            break;
        case CDG_INSN_TILE_BLOCK_XOR:
            print_cdg_insn_tile_block("XOR", (struct cdg_insn_tile_block *) pkt->data);
            break;
        default:
            printf("UNKNOWN INSTRUCTION\n");
            break;
    }
}

// Returns an integer representing the number of milliseconds elapsed since the start of the CDG stream.
inline uint32_t cdg_state_get_time_elapsed(struct cdg_state *state) {
    // 300 packets per second
    return (state->subchannel_packet_count * 1000) / 300;
}

// Returns: 1 if we need to update the framebuffer
int cdg_state_process_insn(struct cdg_state *state, struct subchannel_packet *pkt) {
    uint8_t code;
    struct cdg_insn *insn;

    state->subchannel_packet_count++;

    // not a CDG packet
    if ((pkt->command & 0x3F /* 0b111111 */) != 9) {
        return 0;
    }

    code = pkt->instruction;
    insn = (struct cdg_insn *) pkt->data;

    //process_cdg_insn(pkt);

    switch (code) {
        // Load colors 0-7
        case CDG_INSN_LOAD_COLOR_TABLE_00: {
            struct cdg_insn_load_color_table *insn_load_color_table = (struct cdg_insn_load_color_table *) insn;

            for (int i = 0; i < 8; i++) {
                state->color_table[i] = ntohs(insn_load_color_table->spec[i] & 0x3F3F);
            }

            return 1;
        }
            break;

            // Load colors 8-15
        case CDG_INSN_LOAD_COLOR_TABLE_08: {
            struct cdg_insn_load_color_table *insn_load_color_table = (struct cdg_insn_load_color_table *) insn;

            for (int i = 0; i < 8; i++) {
                state->color_table[i + 8] = ntohs(insn_load_color_table->spec[i] & 0x3F3F);
            }

            return 1;
        }
            break;

            // Clear the screen
        case CDG_INSN_MEMORY_PRESET: {
            struct cdg_insn_memory_preset *insn_memory_preset = (struct cdg_insn_memory_preset *) insn;

            // The repeat code is incremented each time the same command is sent.
            // This is to ensure the screen is cleared in a potentially unreliable stream.
            // Since we're reading from a file, we can just check if the repeat code is 0 and only do this once.
            if (insn_memory_preset->repeat == 0) {
                memset(state->framebuffer, insn_memory_preset->color, 300 * 216);
            }

            return 1;
        }
            break;

        case CDG_INSN_BORDER_PRESET: {
            printf("BORDER\n");
            //The border area is the area contained with a
            //rectangle defined by (0,0,300,216) minus the interior pixels which are contained
            //within a rectangle defined by (6,12,294,204).
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
            break;

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
            break;

        case CDG_INSN_SCROLL_PRESET:
        case CDG_INSN_SCROLL_COPY: {
            // TODO: implement - I haven't seen this in any CDG files yet
            printf("they be doing scrolling\n");
            struct cdg_insn_scroll *insn_scroll = (struct cdg_insn_scroll *) insn;
            uint8_t color = insn_scroll->color;
            uint8_t h_scroll = insn_scroll->h_scroll;
            uint8_t v_scroll = insn_scroll->v_scroll;
        }
            break;
        default:
            printf("unexpected insn: %d\n", code);
    }

    return 0;
}

// Returns: 1 if we need to update the framebuffer
int cdg_reader_advance_to(struct cdg_reader *reader, uint32_t timestamp) {
    struct subchannel_packet insn;
    int needsUpdate = 0;

    while (cdg_state_get_time_elapsed(&reader->state) < timestamp) {
        if (!reader->read_callback(reader->userData, &insn)) {
            // End of CDG stream
            reader->eof = 1;
            break;
        }

        // Intentionally using |= here so that cdg_state_process_insn() is always called
        needsUpdate |= cdg_state_process_insn(&reader->state, &insn);
    }

    return needsUpdate;
}

int testMain(int argc, char *argv[]) {
    struct subchannel_packet pkt;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    FILE *fp;

    fp = fopen(argv[1], "r");

    if (!fp) {
        fprintf(stderr, "failed to open file\n");
        return 1;
    }

    while (!feof(fp) && read_subchannel_packet(fp, &pkt)) {
        if ((pkt.command & 0x3F /* 0b111111 */) == 9) {
            process_cdg_insn(&pkt);
        }
    }

    fclose(fp);

    return 0;
}
