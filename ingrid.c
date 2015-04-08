#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#include <portaudio.h>
#include <portmidi.h>
#include <sndfile.h>

/* generic constants */
#define FALSE 0
#define TRUE 1
#define ERROR -1
typedef unsigned char BYTE;

/* midi constants */
#define NOTEON  0x90
#define NOTEOFF 0x80
#define CC      0xB0
#define CHAN(x) ((x)-1)

/* midi buffer size */
#define BUFFSIZE 64

/* PortMidi & PortAudio abbreviations */
#define PMS PortMidiStream
#define PAS PaStream

/* some launchpad macros and constants */
#define XY2IDX(x,y) (((y)*8)+(x))
#define IDX2X(i) ((i)%8)
#define IDX2Y(i) ((i)/8)
#define COLOR(r,g) ((r)|((g)<<4))

typedef enum {GRID, TOP_ROW, SIDE_COL} lp_ev_zone;
typedef enum {PRESS, RELEASE } lp_ev_type;

typedef struct {
    lp_ev_zone zone;
    int x;
    int y;
    lp_ev_type type;
} lp_event;


int lp_grid_layout[] = 
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
      0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
      0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77 };

int lp_side_col_layout[] = 	
    { 0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78 };

int lp_top_row_layout[] = 
    { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F };


/* struct to keep data of a wav sound */
typedef struct {
  SNDFILE *sndFile;
  SF_INFO sfInfo;
  int position;
  int playing;
} sound;




int find_index(int a[], int num_elements, int value)
{
    int i;
    for (i=0; i<num_elements; i++) {
        if (a[i] == value) return(i);
    }
    return(-1);
}

void find_launchpad(int *inputdev, int *outputdev) {
    const PmDeviceInfo* devinfo;
    int i;
    
    *inputdev = -1;
    *outputdev = -1;

    for (i = 0; i < Pm_CountDevices(); ++i) {
        devinfo = Pm_GetDeviceInfo(i);
        if (strstr(devinfo->name, "Launchpad")) {
            if(devinfo->input && *inputdev == -1) *inputdev = i;
            if(devinfo->output && *outputdev == -1) *outputdev = i;
        }
    }
}


int load_wav(sound* s, char* filename) {
  /* initialize our data structure */
  s->position = 0;
  s->playing = FALSE;
  s->sfInfo.format = 0;
  /* try to open the file */
  s->sndFile = sf_open(filename, SFM_READ, &s->sfInfo);
  if(!s->sndFile) {
	  printf("error opening file\n");
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


int lp_midi_interpret(PmMessage msg, lp_event *ev) {
    int status, d1, d2, idx;
    status = Pm_MessageStatus(msg);
    d1 = Pm_MessageData1(msg);
    d2 = Pm_MessageData2(msg);
    
    if (status == (NOTEON|CHAN(1))) {
        idx = find_index(lp_grid_layout, 64, d1);
        if (idx != -1) {
            ev->zone = GRID;
            ev->x = IDX2X(idx);
            ev->y = IDX2Y(idx);
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
        idx = find_index(lp_side_col_layout, 8, d1);
        if (idx != -1) {
            ev->zone = SIDE_COL;
            ev->x = 0;
            ev->y = idx;
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
    }
    
    if (status == (CC|CHAN(1))) {
        idx = find_index(lp_top_row_layout, 8, d1);
        if (idx != -1) {
            ev->zone = TOP_ROW;
            ev->x = idx;
            ev->y = 0;
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
    }
    return FALSE;
}


void lp_reset(PMS *out) {
    Pm_WriteShort(out, 0, Pm_Message(0xB0, 0, 0));
}

void lp_led(PMS *out, int x, int y, BYTE red, BYTE green) {
    Pm_WriteShort(out, 0, Pm_Message(NOTEON|CHAN(1), 
                                     lp_grid_layout[XY2IDX(x,y)],
                                     COLOR(red, green)));
}


int init_midi(PMS **in, PMS **out) {
    int indev, outdev;
    PmError err_in, err_out;
    Pm_Initialize();
    find_launchpad(&indev, &outdev);
    if (indev != -1 && outdev != -1) {
        err_in  = Pm_OpenInput(in, indev, NULL, BUFFSIZE, NULL, 0);
        err_out = Pm_OpenOutput(out, outdev, NULL, BUFFSIZE, NULL, NULL, 0);
        if (err_in == pmNoError && err_out == pmNoError) {
            return TRUE;
        } else {
            printf("error: %s\n%s\n", 
                (err_in  != pmNoError) ? Pm_GetErrorText(err_in) : "",
                (err_out != pmNoError) ? Pm_GetErrorText(err_out) : "");
            return FALSE;
        }
     } else {
        printf("error: could not find launchpad midi device\n");
        return FALSE;
    }
}


int init_audio(PAS **out, int channels, int samplerate) {
    Pa_Initialize();
    PaStreamParameters params;
    PaError err;
  
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


int read_chunk(sound *s, long frames, float *b, float amp, int mix) {
	  float temp[44100];
	  long i;
	  sf_seek(s->sndFile, s->position, SEEK_SET);
	  sf_readf_float(s->sndFile, temp, frames);
      
	  for(i=0; i<frames; i++) {
		if(mix) {
		  b[i*2] += temp[i*2] * amp;
		  b[i*2+1] += temp[i*2+1] * amp;
		} else {
		  b[i*2] = temp[i*2] * amp;
		  b[i*2+1] = temp[i*2+1] * amp;
		}
	  }
	  s->position += frames;
      return (s->position >= s->sfInfo.frames);
}


void react(PMS *out, lp_event ev, sound sounds[]) {
    int idx;
    if(ev.zone == GRID) {
        if (ev.type == PRESS) {
            idx = XY2IDX(ev.x, ev.y);
            if (sounds[idx].sndFile) {
                lp_led(out, ev.x, ev.y, 0, 3);
                sounds[idx].playing = TRUE;
                sounds[idx].position = 0;
            }
        }
    }
}


void audio_fill(PAS *out, PMS *midiout, int frames, sound sounds[]) {
    int i, first_sound, finished;
    float buffer[44100];
    int to_turn_off[64];
    
    for(i=0;i<64;i++) to_turn_off[i] = FALSE;
    
    first_sound = TRUE;
    for (i = 0; i < 64; ++i) {
        if (sounds[i].playing && sounds[i].sndFile) {
            finished = read_chunk(&sounds[i], frames, buffer, 0.333, !first_sound);
            if (finished) {
                sounds[i].playing = FALSE;
                to_turn_off[i] = TRUE;
            }
            first_sound = FALSE;
        }
    }
    /* very basic limiter */
    for(i=0;i<frames;i++) {
        buffer[i*2] =tanh(buffer[i*2]);
        buffer[i*2+1] =tanh(buffer[i*2+1]);
    }
    if(first_sound) memset(buffer, 0, sizeof(float) * 44100);
    Pa_WriteStream(out, buffer, frames);

    for (i = 0; i < 64; ++i) {
        if (to_turn_off[i]) lp_led(midiout, IDX2X(i), IDX2Y(i), 0, 0);
    }
}


int main(void) {
    int count, dry_run, loaded;
    PMS *midi_in = NULL;
    PMS *midi_out = NULL;
    PaStream *audio_out = NULL;

    PmEvent inbuff[BUFFSIZE];
    int i;
    
    sound sounds[64];
    lp_event ev;

    for(i=0;i<64;i++) sounds[i].sndFile = NULL;
    loaded = load_sounds(sounds);
    printf("loaded %d sounds\n", loaded);

    if (init_midi(&midi_in, &midi_out) && init_audio(&audio_out, 2, 44100)) {

        lp_reset(midi_out);
        Pa_StartStream(audio_out);
        while(1) {
            dry_run = TRUE;

            count = Pa_GetStreamWriteAvailable(audio_out);
            if(count > 0) {
                audio_fill(audio_out, midi_out, count, sounds);
                dry_run = FALSE;
            }

            if(Pm_Poll(midi_in)) {
                count = Pm_Read(midi_in, inbuff, BUFFSIZE);
                for (i=0; i<count; i++) {
                    if (lp_midi_interpret(inbuff[i].message, &ev)) {
                        react(midi_out, ev, sounds);
                    }
                }
                dry_run = FALSE;
            }
            
            if(dry_run) { Pa_Sleep(5); }
        }
        Pa_StopStream(audio_out);
        Pm_Close(midi_in);
    }
    Pa_Terminate(); 
    Pm_Terminate();
    return ERROR;
}
