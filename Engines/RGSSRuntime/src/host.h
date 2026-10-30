// The game thread's side of the link to the host (host.c), and what the Linux test host uses.
#ifndef RGSS_HOST_H
#define RGSS_HOST_H

#include "rgss.h"

enum { HOST_KEY_COUNT = 512 };

typedef struct {
    char *gameIni, *saveDir, *logPath, *soundFont, *fallbackSoundFont;
    char **rtps;
    int rtpCount;
} HostLaunch;

typedef struct {
    bool keys[HOST_KEY_COUNT]; // by libretro key code
    float mouseX, mouseY;      // 0..1 over the frame
    bool mouse[3];
    float wheel;               // notches since the last read
} HostInput;

// Runs one game on the game thread, from Game.ini to its end. Defined by the Ruby side (game.c).
void rgss_run_game(const HostLaunch *launch);

// Hands a finished frame over and waits for the host to grant the next one. False when the host is stopping the game.
bool host_present(const Image *frame);
// Waits for the next frame without handing one over, as a frozen screen does.
bool host_wait_frame(void);
bool host_wait_frame_locked(void);
bool host_stop_requested(void);
void host_set_frame_rate(int rate);
// Drops frames owed from a slow stretch, for Graphics.frame_reset.
void host_frame_reset(void);
// The input state since the last call, with quick taps held down once.
void host_input(HostInput *out);
void host_set_problem(const char *problem);

// For the Linux test host: grants one frame and waits until the game hands it over. False once the game has ended.
bool host_test_step(int timeoutMs);

#endif
