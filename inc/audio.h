#ifndef _AUDIO_H_INCLUDED
#define _AUDIO_H_INCLUDED
#include <stdio.h>
#include <pthread.h>
#include <portaudio.h>

#include "util.h"
#include "minimp3_ex.h"

/* Represents a buffer of PCM data */
struct pcm_buffer {
    size_t size;
    size_t capacity;
    size_t index;

    uint16_t *buffer;
};

struct audio_state {
    /* PCM data buffer */
    struct pcm_buffer *pcm;

    /* MP3 decoding stuff */
    mp3dec_t mp3d;
    mp3dec_file_info_t mp3_file_info;

    /* PortAudio stuff */
    PaStream *stream;

    /* The thread that the MP3 is being played on */
    pthread_t thread;

    ATOMIC_INT timestamp; /* In milliseconds */
    ATOMIC_INT seek_to;   /* In samples */
};

/* Construct a new audio state */
struct audio_state *audio_state_new(void);

/* Free an audio state */
void audio_state_free(struct audio_state *state);

/* Load an MP3 file into the audio state. This decodes the whole file. */
int audio_state_load_file(struct audio_state *state, const char *path, MP3D_PROGRESS_CB progress_cb);

/* Returns a value in milliseconds since the start of the MP3 */
int audio_state_get_pos(struct audio_state *state);

/* Seek to a position in milliseconds */
void audio_state_seek(struct audio_state *state, uint32_t ms);

/* Start playback - returns once playback is complete or an error occurs */
int audio_do_playback(struct audio_state *state);


#endif // _AUDIO_H_INCLUDED
