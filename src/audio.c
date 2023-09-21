#include "audio.h"

#include <stdio.h>
#include <unistd.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "util.h"

struct pcm_buffer *pcm_buffer_new(size_t size) {
    struct pcm_buffer *pcm;

    pcm = (struct pcm_buffer *) malloc(sizeof(struct pcm_buffer));

    CHECK_MEM(pcm)

    pcm->size = 0;
    pcm->capacity = size;
    pcm->buffer = (uint16_t *) malloc(size * sizeof(uint16_t));

    CHECK_MEM(pcm->buffer)

    return pcm;
}

void high_low_buffer_append(struct pcm_buffer *pcm, uint16_t *buf, size_t size) {
    if (pcm->size + size > pcm->capacity) {
        pcm->capacity = pcm->size + size;
        pcm->buffer = (uint16_t *) realloc(pcm->buffer, sizeof(uint16_t) * pcm->capacity);

        CHECK_MEM(pcm->buffer)
    }

    memcpy(pcm->buffer + pcm->size, buf, size * sizeof(uint16_t));
    pcm->size += size;
}

void pcm_buffer_consume(struct pcm_buffer *pcm, size_t size, uint16_t *buf) {
    if (size > pcm->size) {
        fprintf(stderr, "pcm_buffer_consume(): size > pcm->size\n");
        exit(1);
    }

    pcm->size -= size;

    memcpy(buf, pcm->buffer, size * sizeof(uint16_t));
    memmove(pcm->buffer, pcm->buffer + size, pcm->size * sizeof(uint16_t));
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
    UNUSED(inputBuffer); UNUSED(statusFlags);

    struct audio_state *state = (struct audio_state *) userData;
    PaTime timestamp = timeInfo->outputBufferDacTime;

    int channels = state->headerInfo.channels;

    while (state->pcm->size < (frameCount * channels)) {
        mp3dec_frame_info_t info;
        mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

        int samples = mp3dec_decode_frame(&state->mp3d, state->buffer + state->buffer_offset, state->buffer_size - state->buffer_offset, pcm, &info);

        if (samples) {
            state->buffer_offset += info.frame_bytes;
            high_low_buffer_append(state->pcm, (uint16_t *) pcm, samples * channels);
        } else {
            break;
        }
    }

    pcm_buffer_consume(state->pcm, frameCount * channels, (uint16_t *) outputBuffer);

    ATOMIC_INT_SET(state->timestamp, (int) (timestamp * 1000));

    return paContinue;
}

/* Decode the first frame of the MP3 file to get the header information */
static void decode_first_frame(struct audio_state *state) {
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

    int samples = mp3dec_decode_frame(&state->mp3d, state->buffer, state->buffer_size, pcm, &(state->headerInfo));

    if (samples) {
        state->buffer_offset += state->headerInfo.frame_bytes;
        high_low_buffer_append(state->pcm, (uint16_t *) pcm, samples * sizeof(int16_t));
    }
}

void audio_state_seek(struct audio_state *state, uint32_t ms) {
    int samples = (int) ((ms / 1000.0) * state->headerInfo.hz);
    // use mp3dec_ex_seek

}

int play_mp3(const char *path, struct audio_state *state) {
    memset(state, 0, sizeof(struct audio_state));

    if (!read_file(path, (char **) &state->buffer, &state->buffer_size)) {
        fprintf(stderr, "failed to read file\n");
        return 0;
    }

    state->pcm = pcm_buffer_new(2048);
    mp3dec_init(&state->mp3d);
    decode_first_frame(state);

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
