/*
 * DMC1 PC Port - Main Entry Point
 *
 * Платформенная оболочка порта Devil May Cry 1 (PS2) под x86_64 Windows.
 *
 * Соответствие подсистем:
 *   PS2 GS (Graphical Synthesizer) -> OpenGL
 *   PS2 SPU2 (Sound Processor)     -> OpenAL (пока заглушка)
 *   PS2 SIO (Input)                -> Win32
 *   PS2 CDROM (filesystem)         -> NTFS
 *
 * Игровая логика, лоадеры ассетов и рендер моделей удалены: прежние версии
 * опирались на выдуманные заголовки форматов. Сначала форматы нужно разобрать
 * по фактическим данным с диска.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <GL/gl.h>

#include "sys/win32.h"
#include "input/gamepad.h"
#include "render/opengl.h"
#include "audio/openal.h"

typedef struct {
    int running;

    Win32State*     win32;
    OpenGLRenderer* gl;
    GamepadState*   input;
    OpenALAudio*    audio;
} AppState;

static AppState g_app;

static int App_Init(void)
{
    printf("=== DMC1 PC Port : engine shell ===\n");

    g_app.win32 = Win32_Create("Devil May Cry", 1280, 720, 32);
    if (!g_app.win32) {
        printf("ERROR: cannot create window\n");
        return 0;
    }

    g_app.gl = OpenGL_Create((void*)g_app.win32->hwnd);
    if (!g_app.gl) {
        printf("ERROR: cannot create OpenGL context\n");
        MessageBoxA(NULL, "Failed to create OpenGL context.\n\nMake sure your GPU drivers are installed.",
                    "Graphics Error", MB_ICONERROR);
        return 0;
    }
    OpenGL_SetViewport(g_app.gl, g_app.win32->width, g_app.win32->height);

    g_app.input = Gamepad_Init();
    if (!g_app.input) {
        printf("ERROR: cannot initialize input\n");
        return 0;
    }

    g_app.audio = OpenAL_Create();   /* NULL — звука нет, это допустимо */

    printf("GL version : %s\n", glGetString(GL_VERSION));
    printf("GL renderer: %s\n", glGetString(GL_RENDERER));
    printf("Shell ready. Window is open.\n");

    return 1;
}

static void App_Shutdown(void)
{
    if (g_app.audio) OpenAL_Destroy(g_app.audio);
    if (g_app.input) Gamepad_Destroy(g_app.input);
    if (g_app.gl)    OpenGL_Destroy(g_app.gl);
    if (g_app.win32) Win32_Destroy(g_app.win32);
    memset(&g_app, 0, sizeof(g_app));
}

static void App_Frame(void)
{
    /* Держим viewport в соответствии с фактическим размером окна */
    if (g_app.gl && (g_app.gl->width != g_app.win32->width ||
                     g_app.gl->height != g_app.win32->height)) {
        OpenGL_SetViewport(g_app.gl, g_app.win32->width, g_app.win32->height);
    }

    Gamepad_Update(g_app.input, g_app.win32);

    OpenGL_Clear(g_app.gl, 0.07f, 0.07f, 0.09f, 1.0f);

    Win32_SwapBuffers(g_app.win32);
}

static void App_MainLoop(void)
{
    while (g_app.running && Win32_IsActive(g_app.win32)) {
        Win32_ProcessMessages();
        if (!Win32_IsActive(g_app.win32)) break;
        App_Frame();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    if (!App_Init()) {
        MessageBoxA(NULL, "Failed to initialize DMC1 PC Port", "Error", MB_ICONERROR);
        App_Shutdown();
        return 1;
    }

    g_app.running = 1;
    App_MainLoop();

    App_Shutdown();
    return 0;
}
