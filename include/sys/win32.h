/*
 * DMC1 PC Port - Win32 Window System Header
 */

#ifndef DMC1_WIN32_H
#define DMC1_WIN32_H

#include <stddef.h>

/* Include Windows headers for HWND/HDC types */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02

typedef struct {
    HWND       hwnd;
    int        width;
    int        height;
    int        bpp;
    int        active;
    int        mouse_x;
    int        mouse_y;
    int        mouse_buttons;
    int        keys[256];
} Win32State;

/* Create a new window */
Win32State* Win32_Create(const char* title, int width, int height, int bpp);

/* Process Windows messages */
void Win32_ProcessMessages(void);

/* Check if window is active */
int Win32_IsActive(Win32State* state);

/* Destroy window */
void Win32_Destroy(Win32State* state);

/* Get current time in seconds */
double Win32_GetTime(void);

/* Swap front/back buffers */
void Win32_SwapBuffers(Win32State* state);

#endif /* DMC1_WIN32_H */
