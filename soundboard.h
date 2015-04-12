#ifndef SOUNDBOARD_H_
#define SOUNDBOARD_H_

int init_soundboard(void **state, launchpad *lp, ms t);
void soundboard_react(launchpad *lp, lp_event ev, void *state, ms t);
void soundboard_fill_audio(float* buff, int frames, launchpad *lp, void *state);
void soundboard_tick(void *state, launchpad *lp, ms t);

#endif /* SOUNDBOARD_H_ */
