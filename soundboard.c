#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <math.h>
#include <sndfile.h>

#include "constants.h"
#include "launchpad.h"
#include "soundboard.h"

#define SAMPLE_DIR "samples"

/* struct to keep data of a wav sound */
typedef struct {
    SNDFILE *sndFile;
    SF_INFO sfInfo;
    int position;
    int playing;
} sound;


/* whole state of the soundboard */
typedef struct {
    sound sounds[64];
} sb_state;


int load_wav(sound *s, char *filename) {
    /* initialize our data structure */
    s->position = 0;
    s->playing = FALSE;
    s->sfInfo.format = 0;
    /* try to open the file */
    s->sndFile = sf_open(filename, SFM_READ, &s->sfInfo);
    if (!s->sndFile) {
        fprintf(stderr, "error opening file: %s\n", filename);
        return FALSE;
    } else {
        return TRUE;
    }
}


int load_sounds(sound sounds[]) {
    int loaded = 0, n, i;
    struct dirent **namelist;
    char filename[300];

    n = scandir(SAMPLE_DIR, &namelist, NULL, alphasort);
    if (n < 0) {
        return (ERROR);
    }
    for (i = 0; i < n && loaded < 64; ++i) {
        if (strstr(namelist[i]->d_name, ".wav")) {
            sprintf(filename, "%s/%s", SAMPLE_DIR, namelist[i]->d_name);
            if (load_wav(&sounds[loaded], filename)) {
                loaded += 1;
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    return loaded;
}


int init_soundboard(void **untyped_state, launchpad *lp, ms t) {
    int loaded, i;
    sb_state *state;
    UNUSED(t);
    UNUSED(lp);

    *untyped_state = malloc(sizeof(sb_state));
    state = (sb_state *)*untyped_state;

    for (i = 0; i < 64; i++)
        state->sounds[i].sndFile = NULL;
    loaded = load_sounds(state->sounds);
    printf("soundboard: loaded %d sounds\n", loaded);
    if (loaded == ERROR) {
        return FALSE;
    } else {
        return TRUE;
    }
}


void soundboard_react(launchpad *lp, lp_event ev, void *untyped_state, ms t) {
    int idx;
    sb_state *state = (sb_state *)untyped_state;
    UNUSED(t);

    if (ev.zone == GRID) {
        if (ev.type == PRESS) {
            idx = XY2IDX(ev.x, ev.y);
            if (state->sounds[idx].sndFile) {
                lp_led(lp, ev.x, ev.y, 0, 3);
                state->sounds[idx].playing = TRUE;
                state->sounds[idx].position = 0;
            }
        }
    }
}


int read_chunk(sound *s, long frames, float *b, float amp) {
    float temp[44100];
    long i, sndfrms, remaining, tocopy;
    sf_seek(s->sndFile, s->position, SEEK_SET);
    sf_readf_float(s->sndFile, temp, frames);

    sndfrms = s->sfInfo.frames;
    remaining = sndfrms - s->position;
    tocopy = (frames > remaining) ? remaining : frames;
    for (i = 0; i < tocopy; i++) {
        b[i * 2] += temp[i * 2] * amp;
        b[i * 2 + 1] += temp[i * 2 + 1] * amp;
    }
    s->position += frames;
    return (s->position >= s->sfInfo.frames);
}


void soundboard_fill_audio(float *buff, int frames, launchpad *lp,
                           void *untyped_state) {
    int i, finished;
    int to_turn_off[64];
    sb_state *state = (sb_state *)untyped_state;

    for (i = 0; i < 64; i++)
        to_turn_off[i] = FALSE;

    for (i = 0; i < 64; ++i) {
        if (state->sounds[i].playing && state->sounds[i].sndFile) {
            finished = read_chunk(&(state->sounds[i]), frames, buff, 0.5);
            if (finished) {
                state->sounds[i].playing = FALSE;
                to_turn_off[i] = TRUE;
            }
        }
    }
    /* very basic limiter */
    for (i = 0; i < frames; i++) {
        buff[i * 2] = tanh(buff[i * 2]);
        buff[i * 2 + 1] = tanh(buff[i * 2 + 1]);
    }

    for (i = 0; i < 64; ++i) {
        if (to_turn_off[i])
            lp_led(lp, IDX2X(i), IDX2Y(i), 0, 0);
    }
}

void soundboard_tick(void *untyped_state, launchpad *lp, ms t) {
    UNUSED(untyped_state);
    UNUSED(lp);
    UNUSED(t);
}
