#ifndef _AUDIO_H_INCLUDED
#define _AUDIO_H_INCLUDED
#include <stdio.h>
#include <pthread.h>
#include <portaudio.h>

#include "util.h"
#include "minimp3.h"

struct pcm_buffer {
    size_t size;
    size_t capacity;

    uint16_t *buffer;
};

struct audio_state {
    /* MP3 data buffer */
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_offset;

    /* PCM data buffer */
    struct pcm_buffer *pcm;

    mp3dec_t mp3d;
    mp3dec_frame_info_t headerInfo;
    pthread_t thread;
    PaStream *stream;

    ATOMIC_INT timestamp; /* In milliseconds */
};

void audio_state_seek(struct audio_state *state, uint32_t ms);
int play_mp3(const char *path, struct audio_state *state);


#endif // _AUDIO_H_INCLUDED
