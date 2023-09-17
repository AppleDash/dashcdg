#include <stdio.h>

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
        if ((pkt.command & 0b111111) == 9) {
            process_cdg_insn(&pkt);
        }
    }

    fclose(fp);

    return 0;
}
