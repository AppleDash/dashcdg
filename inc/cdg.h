#ifndef _CDG_H_INCLUDED
#define _CDG_H_INCLUDED

#include <stdint.h>

#define CDG_INSN_INVALID             -2
#define CDG_INSN_UNKNOWN             -1
#define CDG_INSN_MEMORY_PRESET       1
#define CDG_INSN_BORDER_PRESET       2
#define CDG_INSN_TILE_BLOCK          6
#define CDG_INSN_SCROLL_PRESET       20
#define CDG_INSN_SCROLL_COPY         24
#define CDG_INSN_DEF_TRANSPARENT     28
#define CDG_INSN_LOAD_COLOR_TABLE_00 30
#define CDG_INSN_LOAD_COLOR_TABLE_08 31
#define CDG_INSN_TILE_BLOCK_XOR      38

#define ARRAY_INDEX(X, Y) (((Y) * 300) + (X))
// 300 frames per second
#define MS_TO_CDG_FRAME_COUNT(X) ((int)(((float)(X) * 300.0f) / 1000.0f))
#define CDG_FRAME_COUNT_TO_MS(X) (((float)(X) * 1000.0f) / 300.0f)

typedef int cdg_ts_t;

#pragma pack(push, 1)
struct subchannel_packet {
    uint8_t command;
    uint8_t instruction;
    uint8_t parity_q[2];
    uint8_t data[16];
    uint8_t parity_p[4];
};

struct cdg_insn {
    uint8_t _data[16];
};

struct cdg_insn_memory_preset {
    uint8_t color;       // only lower 4 bits are used
    uint8_t repeat;      // only lower 4 bits are used
    uint8_t _filler[14];
};

struct cdg_insn_border_preset {
    uint8_t color;       // only lower 4 bits are used
    uint8_t _filler[15];
};

struct cdg_insn_tile_block {
    uint8_t color_0;     // only lower 4 bits are used
    uint8_t color_1;     // only lower 4 bits are used
    uint8_t row;         // only lower 5 bits are used
    uint8_t column;      // only lower 6 bits are used
    uint8_t pixels[12];  // only lower 6 bits of each byte are used
};

struct cdg_insn_scroll {
    uint8_t color;       // only lower 4 bits are used
    uint8_t h_scroll;    // only lower 6 bits are used
    uint8_t v_scroll;    // only lower 6 bits are used
    uint8_t _filler[13];
};

struct cdg_insn_define_transparent {
    uint8_t _filler[16];
};

struct cdg_insn_load_color_table {
    uint16_t spec[8];    // AND with 0x3F3F to clear P and Q channel
};
#pragma pack(pop)

// return: 0 -> eof, 1 -> read an instruction, 2 -> read an empty subchannel packet
typedef int (*cdg_reader_read_callback)(void *userData, struct subchannel_packet *outPkt);

struct cdg_keyframe {
    cdg_ts_t timestamp;      // subchannel packet count
    int color_table[16];
    uint8_t clear_color;
};

struct cdg_keyframe_list {
    size_t count;
    struct cdg_keyframe *keyframes;
};

struct cdg_state {
    cdg_ts_t subchannel_packet_count;
    int color_table[16];
    unsigned int framebuffer[300 * 216];
};

struct cdg_reader {
    int eof;
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_index;

    struct cdg_state state;
    struct cdg_keyframe_list keyframes;
};

/* Get the time elapsed in milliseconds since the start of the CDG file */
uint32_t cdg_state_get_time_elapsed(struct cdg_state *state);

/* Process an instruction and update the state */
int cdg_state_process_insn(struct cdg_state *state, struct subchannel_packet *pkt);

/* Initialize a CDG reader */
struct cdg_reader *cdg_reader_new(void);

/* Free a CDG reader */
void cdg_reader_free(struct cdg_reader *reader);

/* Load a CDG file into a reader */
int cdg_reader_load_file(struct cdg_reader *reader, const char *path);

/* Read a frame from the CDG buffer into the given packet */
int cdg_reader_read_frame(struct cdg_reader *reader, struct subchannel_packet *outPkt);

/* Reset the reader to the beginning of the CDG file */
void cdg_reader_reset(struct cdg_reader *reader);

/* Advance the reader to the given timestamp, processing any commands between the current timestamp and the one we're advancing to. */
int cdg_reader_seek_forward(struct cdg_reader *reader, cdg_ts_t timestamp);

/* Build a list of seek snapshots from the CDG reader */
int cdg_reader_build_keyframe_list(struct cdg_reader *reader);

struct cdg_keyframe *cdg_reader_find_closest_keyframe(struct cdg_keyframe_list *list, cdg_ts_t ts);

/* Seek to the given keyframe */
void cdg_reader_seek_to_keyframe(struct cdg_reader *reader, struct cdg_keyframe *keyframe);
int cdg_reader_seek_bidirectional(struct cdg_reader *reader, cdg_ts_t ts);

#endif // _CDG_H_INCLUDED
