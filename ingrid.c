#include <stdio.h>
#include <string.h>
#include <time.h>
#include <portaudio.h>

#include "constants.h"
#include "launchpad.h"
#include "soundboard.h"


ms millitime() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}


int init_audio(PaStream ** out, int channels, int samplerate) {
    PaStreamParameters params;
    PaError err;

    Pa_Initialize();

    params.device = Pa_GetDefaultOutputDevice();
    params.channelCount = channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = 0.025;
    params.hostApiSpecificStreamInfo = 0;

    /* parameters for Pa_OpenStream 
     * out -> output stream
     * 0   -> no input
     * &params -> stream parameters, populated above
     * samplerate -> output sample rate
     * paFramesPerBufferUnspecified -> let portaudio choose the buffersize
     * paNoFlag -> no special modes (clip off, dither off)
     * NULL -> no callback
     * NULL -> ?                        */
    err =
        Pa_OpenStream(out, 0, &params, samplerate,
                      paFramesPerBufferUnspecified, paNoFlag, NULL, NULL);
    if (err) {
        fprintf(stderr, "error opening portaudio stream, error code = %i\n",
                err);
        return FALSE;
    } else {
        return TRUE;
    }
}


void fill_audio(PaStream * out, int frames, launchpad *lp, void *state) {
    float buffer[44100];
    memset(buffer, 0, sizeof(float) * 44100);
    soundboard_fill_audio(buffer, frames, lp, state);
    Pa_WriteStream(out, buffer, frames);
}


int main(void) {
    int count, dry_run;
    PaStream *audio_out = NULL;
    void *state = NULL;

    launchpad lp;
    lp_event ev;

    if (init_launchpad(&lp) &&
        init_audio(&audio_out, 2, 44100) &&
        init_soundboard(&state, &lp, millitime())) {

        Pa_StartStream(audio_out);
        while (TRUE) {
            dry_run = TRUE;

            soundboard_tick(state, &lp, millitime());

            count = Pa_GetStreamWriteAvailable(audio_out);
            if (count > 0) {
                fill_audio(audio_out, count, &lp, state);
                dry_run = FALSE;
            }
            while (lp_get_next_event(&lp, &ev)) {
                soundboard_react(&lp, ev, state, millitime());
                dry_run = FALSE;
            }

            if (dry_run) {
                Pa_Sleep(3);
            }
        }
        Pa_StopStream(audio_out);
        Pa_Terminate();
        close_launchpad(&lp);
    }
    return ERROR;
}
