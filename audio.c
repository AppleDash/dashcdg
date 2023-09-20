#include "audio.h"

#include <stdio.h>
#include <pthread.h>
#include <soundio/soundio.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "util.h"

struct high_low_buffer *high_low_buffer_new(size_t size) {
    struct high_low_buffer *hlb;

    hlb = (struct high_low_buffer *) malloc(sizeof(struct high_low_buffer));

    if (hlb == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        exit(1);
    }

    hlb->lowWaterMark = 0;
    hlb->size = 0;
    hlb->capacity = size;
    hlb->buffer = (uint16_t *) malloc(size * sizeof(uint16_t));

    if (hlb->buffer == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        exit(1);
    }

    return hlb;
}

void high_low_buffer_append(struct high_low_buffer *hlb, uint16_t *buf, size_t size) {
    if (hlb->size + size > hlb->capacity) {
        hlb->capacity = hlb->size + size;
        hlb->buffer = (uint16_t *) realloc(hlb->buffer, sizeof(uint16_t) * hlb->capacity);

        if (hlb->buffer == NULL) {
            fprintf(stderr, "failed to allocate memory\n");
            exit(1);
        }
    }

    memcpy(hlb->buffer + hlb->size, buf, size * sizeof(uint16_t));
    hlb->size += size;
}

void high_low_buffer_consume(struct high_low_buffer *hlb, size_t size, uint16_t *buf) {
    if (size > hlb->size) {
        fprintf(stderr, "high_low_buffer_consume(): size > hlb->size\n");
        exit(1);
    }

    hlb->size -= size;

    memcpy(buf, hlb->buffer, size * sizeof(uint16_t));
    memmove(hlb->buffer, hlb->buffer + size, hlb->size * sizeof(uint16_t));
}

static int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                      void *userData);

static int create_pa_stream(struct audio_state *state) {
    PaStream *stream;
    PaError err;

    if ((err = Pa_Initialize()) != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 0;
    }

    if ((err = Pa_OpenDefaultStream(&stream, 0, state->headerInfo.channels, paInt16, state->headerInfo.hz, paFramesPerBufferUnspecified, paCallback, state)) != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 0;
    }

    if ((err = Pa_StartStream(stream)) != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 0;
    }

    state->stream = stream;

    return 1;
}

static int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                        void *userData) {
    struct audio_state *state = (struct audio_state *) userData;
    PaTime timestamp = timeInfo->outputBufferDacTime;

    int channels = state->headerInfo.channels;

    while (state->hlb->size < (frameCount * channels)) {
        mp3dec_frame_info_t info;
        mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

        int samples = mp3dec_decode_frame(&state->mp3d, state->buffer + state->buffer_offset, state->buffer_size - state->buffer_offset, pcm, &info);

        if (samples) {
            state->buffer_offset += info.frame_bytes;
            high_low_buffer_append(state->hlb, (uint16_t *) pcm, samples * channels);
        } else {
            break;
        }
    }

    high_low_buffer_consume(state->hlb, frameCount * channels, (uint16_t *) outputBuffer);

    ATOMIC_INT_SET(state->timestamp, (int) (timestamp * 1000));

    return paContinue;
}

/* Decode the first frame of the MP3 file to get the header information */
static void decode_first_frame(struct audio_state *state) {
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

    int samples = mp3dec_decode_frame(&state->mp3d, state->buffer, state->buffer_size, pcm, &(state->headerInfo));

    if (samples) {
        state->buffer_offset += state->headerInfo.frame_bytes;
        high_low_buffer_append(state->hlb, (uint16_t *) pcm, samples * sizeof(int16_t));
    }
}

int start_mp3_playback(const char *path, struct audio_state *state) {
    memset(state, 0, sizeof(struct audio_state));

    if (!read_file(path, &state->buffer, &state->buffer_size)) {
        fprintf(stderr, "failed to read file\n");
        return 0;
    }

    state->hlb = high_low_buffer_new(2048);
    mp3dec_init(&state->mp3d);
    decode_first_frame(state);

    if (!create_pa_stream(state)) {
        fprintf(stderr, "failed to create PortAudio stream\n");
        return 0;
    }

//    while (Pa_IsStreamActive(state->stream)) {
//        Pa_Sleep(100);
//    }

    return 1;
}
