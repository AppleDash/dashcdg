#ifndef _AUDIO_H_INCLUDED
#define _AUDIO_H_INCLUDED
#include <stdio.h>
#include <pthread.h>
#include <portaudio.h>

#include "util.h"
#include "minimp3_ex.h"

struct pcm_buffer {
    size_t size;
    size_t capacity;
    size_t index;

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
    mp3dec_file_info_t fileInfo;
    pthread_t thread;
    PaStream *stream;

    ATOMIC_INT timestamp; /* In milliseconds */
    ATOMIC_INT seek_to;   /* In samples */
};

struct audio_state *audio_state_new(void);
void audio_state_free(struct audio_state *state);
int audio_state_load_file(struct audio_state *state, const char *path, MP3D_PROGRESS_CB progress_cb);
int audio_state_get_pos(struct audio_state *state);
void audio_state_seek(struct audio_state *state, uint32_t ms);
int audio_do_playback(struct audio_state *state);


#endif // _AUDIO_H_INCLUDED
