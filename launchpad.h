
#define XY2IDX(x,y) (((y)*8)+(x))
#define IDX2X(i) ((i)%8)
#define IDX2Y(i) ((i)/8)

typedef enum {GRID, TOP_ROW, SIDE_COL} lp_ev_zone;
typedef enum {PRESS, RELEASE } lp_ev_type;

typedef struct {
    lp_ev_zone zone;
    int x;
    int y;
    lp_ev_type type;
} lp_event;

typedef struct {
    void* in;
    void* out;
} launchpad;

int init_launchpad(launchpad *lp);
void close_launchpad(launchpad *lp);
void lp_led(launchpad *lp, int x, int y, BYTE red, BYTE green);
void lp_clear(launchpad *lp);
int lp_get_next_event(launchpad *lp, lp_event *event);
