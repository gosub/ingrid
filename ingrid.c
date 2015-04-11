#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#include <portaudio.h>
#include <sndfile.h>

#include "launchpad.h"

/* generic constants */
#define FALSE 0
#define TRUE 1
#define ERROR -1


/* PortAudio abbreviations */
#define PAS PaStream

/* struct to keep data of a wav sound */
typedef struct {
  SNDFILE *sndFile;
  SF_INFO sfInfo;
  int position;
  int playing;
} sound;


int load_wav(sound* s, char* filename) {
  /* initialize our data structure */
  s->position = 0;
  s->playing = FALSE;
  s->sfInfo.format = 0;
  /* try to open the file */
  s->sndFile = sf_open(filename, SFM_READ, &s->sfInfo);
  if(!s->sndFile) {
	  printf("error opening file: %s\n", filename);
	  return FALSE;
  } else {
	return TRUE;
  }
}


int load_sounds(sound sounds[]) {
    int loaded = 0;
    DIR *dirp;
    struct dirent *dp;
    char filename[300];
    
    dirp = opendir("samples");
    if (dirp == NULL)
        return (ERROR);
    while (((dp = readdir(dirp)) != NULL) && loaded<64) {
        if (strstr(dp->d_name, ".wav")) {
            strcpy(filename, "samples");
            strcat(filename, "/");
            strcat(filename, dp->d_name);
            printf("found sample: %s\n", filename);
            if(load_wav(&sounds[loaded], filename)) 
                loaded += 1;
        }
    }
    (void)closedir(dirp);
    return loaded;
}




int init_audio(PAS **out, int channels, int samplerate) {
    PaStreamParameters params;
    PaError err;

    Pa_Initialize();
  
    params.device = Pa_GetDefaultOutputDevice();
    params.channelCount = channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = 0.025;
    params.hostApiSpecificStreamInfo = 0;

    err = Pa_OpenStream(out,
                        0,  /* no input */
                        &params,
                        samplerate,
                        paFramesPerBufferUnspecified,  /* let portaudio choose the buffersize */
                        paNoFlag,  /* no special modes (clip off, dither off) */
                        NULL,  /* no callback function */
                        NULL );
    if (err) {
        printf("error opening portaudio stream, error code = %i\n", err);
        return FALSE;
    } else {
        return TRUE;
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
    for(i=0; i<tocopy; i++) {
        b[i*2] += temp[i*2] * amp;
        b[i*2+1] += temp[i*2+1] * amp;
    }
    s->position += frames;
    return (s->position >= s->sfInfo.frames);
}


void react(launchpad *lp, lp_event ev, sound sounds[]) {
    int idx;
    if(ev.zone == GRID) {
        if (ev.type == PRESS) {
            idx = XY2IDX(ev.x, ev.y);
            if (sounds[idx].sndFile) {
                lp_led(lp, ev.x, ev.y, 0, 3);
                sounds[idx].playing = TRUE;
                sounds[idx].position = 0;
            }
        }
    }
}


void audio_fill(PAS *out, launchpad *lp, int frames, sound sounds[]) {
    int i, finished;
    float buffer[44100];
    int to_turn_off[64];
    
    for(i=0;i<64;i++) to_turn_off[i] = FALSE;
    memset(buffer, 0, sizeof(float) * 44100);

    for (i = 0; i < 64; ++i) {
        if (sounds[i].playing && sounds[i].sndFile) {
            finished = read_chunk(&sounds[i], frames, buffer, 0.333);
            if (finished) {
                sounds[i].playing = FALSE;
                to_turn_off[i] = TRUE;
            }
        }
    }
    /* very basic limiter */
    for(i=0;i<frames;i++) {
        buffer[i*2] =tanh(buffer[i*2]);
        buffer[i*2+1] =tanh(buffer[i*2+1]);
    }
    Pa_WriteStream(out, buffer, frames);

    for (i = 0; i < 64; ++i) {
        if (to_turn_off[i]) lp_led(lp, IDX2X(i), IDX2Y(i), 0, 0);
    }
}


int main(void) {
    int count, dry_run, loaded;
    PaStream *audio_out = NULL;
    int i;
    
    sound sounds[64];
    launchpad lp;
    lp_event ev;

    for(i=0;i<64;i++) sounds[i].sndFile = NULL;
    loaded = load_sounds(sounds);
    printf("loaded %d sounds\n", loaded);

    if (init_launchpad(&lp) && init_audio(&audio_out, 2, 44100)) {

        lp_clear(&lp);
        Pa_StartStream(audio_out);
        while(1) {
            dry_run = TRUE;

            count = Pa_GetStreamWriteAvailable(audio_out);
            if(count > 0) {
                audio_fill(audio_out, &lp, count, sounds);
                dry_run = FALSE;
            }
			while (lp_get_next_event(&lp, &ev)) {
				react(&lp, ev, sounds);
				dry_run = FALSE;
			}
            
            if(dry_run) { Pa_Sleep(5); }
        }
        Pa_StopStream(audio_out);
    }
    Pa_Terminate();
    close_launchpad(&lp);
    return ERROR;
}
