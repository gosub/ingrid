#include <stdio.h>
#include <string.h>
#include <portmidi.h>

#include "constants.h"
#include "launchpad.h"


#define COLOR(r,g) ((r)|((g)<<4))

/* midi constants */
#define NOTEON  0x90
#define NOTEOFF 0x80
#define CC      0xB0
#define CHAN(x) ((x)-1)

/* midi buffer size */
#define BUFFSIZE 64

int lp_grid_layout[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77
};

int lp_side_col_layout[] = { 0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78 };

int lp_top_row_layout[] = { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F };


int find_index(int a[], int num_elements, int value) {
    int i;
    for (i = 0; i < num_elements; i++) {
        if (a[i] == value)
            return (i);
    }
    return (NOTFOUND);
}


void find_launchpad(int *inputdev, int *outputdev) {
    const PmDeviceInfo *devinfo;
    int i;

    *inputdev = NOTFOUND;
    *outputdev = NOTFOUND;

    for (i = 0; i < Pm_CountDevices(); ++i) {
        devinfo = Pm_GetDeviceInfo(i);
        if (strstr(devinfo->name, "Launchpad")) {
            if (devinfo->input && *inputdev == NOTFOUND)
                *inputdev = i;
            if (devinfo->output && *outputdev == NOTFOUND)
                *outputdev = i;
        }
    }
}


int lp_midi_interpret(PmMessage msg, lp_event *ev) {
    int status, d1, d2, idx;
    status = Pm_MessageStatus(msg);
    d1 = Pm_MessageData1(msg);
    d2 = Pm_MessageData2(msg);

    if (status == (NOTEON | CHAN(1))) {
        idx = find_index(lp_grid_layout, 64, d1);
        if (idx != NOTFOUND) {
            ev->zone = GRID;
            ev->x = IDX2X(idx);
            ev->y = IDX2Y(idx);
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
        idx = find_index(lp_side_col_layout, 8, d1);
        if (idx != NOTFOUND) {
            ev->zone = SIDE_COL;
            ev->x = 0;
            ev->y = idx;
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
    }

    if (status == (CC | CHAN(1))) {
        idx = find_index(lp_top_row_layout, 8, d1);
        if (idx != NOTFOUND) {
            ev->zone = TOP_ROW;
            ev->x = idx;
            ev->y = 0;
            ev->type = (d2 == 127) ? PRESS : RELEASE;
            return TRUE;
        }
    }
    return FALSE;
}


void lp_clear(launchpad *lp) {
    Pm_WriteShort(lp->out, 0, Pm_Message(0xB0, 0, 0));
}


void lp_led(launchpad *lp, int x, int y, BYTE red, BYTE green) {
    Pm_WriteShort(lp->out, 0, Pm_Message(NOTEON | CHAN(1),
                                         lp_grid_layout[XY2IDX(x, y)],
                                         COLOR(red, green)));
}


void lp_side_col(launchpad *lp, int y, BYTE red, BYTE green) {
    Pm_WriteShort(lp->out, 0, Pm_Message(NOTEON | CHAN(1),
                                         lp_side_col_layout[y],
                                         COLOR(red, green)));
}


void lp_top_row(launchpad *lp, int x, BYTE red, BYTE green) {
    Pm_WriteShort(lp->out, 0, Pm_Message(CC | CHAN(1),
                                         lp_top_row_layout[x],
                                         COLOR(red, green)));
}


int init_launchpad(launchpad *lp) {
    int indev, outdev;
    PmError err_in, err_out;
    Pm_Initialize();
    find_launchpad(&indev, &outdev);
    if (indev != NOTFOUND && outdev != NOTFOUND) {
        err_in = Pm_OpenInput(&(lp->in), indev, NULL, BUFFSIZE, NULL, 0);
        err_out =
            Pm_OpenOutput(&(lp->out), outdev, NULL, BUFFSIZE, NULL, NULL, 0);
        if (err_in == pmNoError && err_out == pmNoError) {
            lp_clear(lp);
            return TRUE;
        } else {
            fprintf(stderr, "error: %s\n%s\n",
                    (err_in != pmNoError) ? Pm_GetErrorText(err_in) : "",
                    (err_out != pmNoError) ? Pm_GetErrorText(err_out) : "");
            return FALSE;
        }
    } else {
        fprintf(stderr, "error: could not find any launchpad midi device\n");
        return FALSE;
    }
}


int lp_get_next_event(launchpad *lp, lp_event *event) {
    PmEvent pme;
    int count, i;

    if (Pm_Poll(lp->in)) {
        count = Pm_Read(lp->in, &pme, 1);
        for (i = 0; i < count; i++) {
            if (lp_midi_interpret(pme.message, event)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}


void close_launchpad(launchpad *lp) {
    Pm_Close(lp->in);
    Pm_Terminate();
}
