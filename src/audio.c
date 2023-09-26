#include "audio.h"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include "util.h"

struct pcm_buffer *pcm_buffer_from(uint16_t *buf, size_t size) {
    struct pcm_buffer *pcm;

    assert(buf != NULL);
    assert(size > 0);

    pcm = (struct pcm_buffer *) malloc(sizeof(struct pcm_buffer));

    CHECK_MEM(pcm)

    pcm->index = 0;
    pcm->size = size;
    pcm->capacity = size;
    pcm->buffer = buf;

    return pcm;
}

void pcm_buffer_consume(struct pcm_buffer *pcm, size_t size, uint16_t *buf) {
    if ((pcm->index + size) > pcm->size) {
        fprintf(stderr, "pcm_buffer_consume(): size > pcm->size (asked for %ld, at index %ld, only have %ld)!\n", size, pcm->index, (pcm->size + pcm->index));
        exit(1);
    }

    memcpy(buf, pcm->buffer + pcm->index, size * sizeof(uint16_t));
    pcm->index += size;
}

void pcm_buffer_free(struct pcm_buffer *pcm) {
    if (pcm) {
        if (pcm->buffer) {
            free(pcm->buffer);
        }

        free(pcm);
    }
}

static int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                      void *userData);

static int create_pa_stream(struct audio_state *state) {
    PaStream *stream;
    PaError err;

    // This business is just to stop PortAudio from spamming the console
    backup_and_close_stdout_stderr();
    if ((err = Pa_Initialize()) != paNoError) {
        restore_stdout_stderr();
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 0;
    }

    restore_stdout_stderr();

    if ((err = Pa_OpenDefaultStream(&stream, 0, state->fileInfo.channels, paInt16, state->fileInfo.hz, paFramesPerBufferUnspecified, paCallback, state)) != paNoError) {
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
    UNUSED(inputBuffer); UNUSED(statusFlags);

    struct audio_state *state = (struct audio_state *) userData;
    int latency = (int) ((timeInfo->outputBufferDacTime - timeInfo->currentTime) * 1000.0); // in ms
    int seekTo; // in frames
    int audioTs; // in ms

    if ((seekTo = ATOMIC_INT_GET(state->seek_to)) != -1) {
        state->pcm->index = seekTo;

        ATOMIC_INT_SET(state->seek_to, -1);
    }

    audioTs = audio_state_get_pos(state) - latency;

    ATOMIC_INT_SET(state->timestamp, audioTs < 0 ? 0 : audioTs);
    pcm_buffer_consume(state->pcm, frameCount * state->fileInfo.channels, (uint16_t *) outputBuffer);

    return paContinue;
}

struct audio_state *audio_state_new(void) {
    struct audio_state *state;

    state = (struct audio_state *) malloc(sizeof(struct audio_state));

    CHECK_MEM(state)

    memset(state, 0, sizeof(struct audio_state));

    state->timestamp = -1;
    state->seek_to = -1;

    return state;
}

void audio_state_free(struct audio_state *state) {
    if (state) {
        if (state->buffer) {
            free(state->buffer);
        }

        if (state->pcm) {
            pcm_buffer_free(state->pcm);
        }

        free(state);
    }
}

int audio_state_load_file(struct audio_state *state, const char *path, MP3D_PROGRESS_CB progress_cb) {
    uint8_t *buffer;
    size_t size;
    int err;

    if (!read_file(path, (char **) &buffer, &size)) {
        fprintf(stderr, "failed to read file\n");
        return 0;
    }

    mp3dec_init(&state->mp3d);

    if ((err = mp3dec_load(&state->mp3d, path, &(state->fileInfo), progress_cb, state)) < 0) {
        fprintf(stderr, "failed to load MP3 file: %d\n", err);
        return 0;
    }

    if (state->pcm) {
        pcm_buffer_free(state->pcm);
    }

    state->pcm = pcm_buffer_from((uint16_t *) state->fileInfo.buffer, state->fileInfo.samples);

    return 1;
}

int audio_state_get_pos(struct audio_state *state) {
    const float samplesPerMs = (float) state->fileInfo.hz / 1000.0F;

    return (int) ((float) state->pcm->index / samplesPerMs / (float) state->fileInfo.channels);
}

void audio_state_seek(struct audio_state *state, uint32_t ms) {
    const float samplesPerMs = (float) state->fileInfo.hz / 1000.0F;

    size_t samples = (size_t) ((float) ms * samplesPerMs * (float) state->fileInfo.channels);

    if (samples > state->fileInfo.samples) {
        samples = state->fileInfo.samples;
        printf("audio_state_seek(): samples > fileInfo.samples, setting to fileInfo.samples.\n");
    }

    ATOMIC_INT_SET(state->seek_to, samples);
}

int audio_do_playback(struct audio_state *state) {
    if (!create_pa_stream(state)) {
        fprintf(stderr, "failed to create PortAudio stream\n");
        return 0;
    }

    while (Pa_IsStreamActive(state->stream)) {
        Pa_Sleep(100);
    }

    pcm_buffer_free(state->pcm);
    Pa_CloseStream(state->stream);

    return 1;
}
