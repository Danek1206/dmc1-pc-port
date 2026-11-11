/*
 * DMC1 PC Port - Main Entry Point
 * 
 * Полный реверс Devil May Cry 1 (PS2) и перекомпиляция под x86_64 Windows
 * 
 * Архитектура:
 *   PS2 GS (Graphical Synthesizer) → OpenGL 4.6
 *   PS2 SPU2 (Sound Processor)     → OpenAL
 *   PS2 SIO (Input)                → Win32 + DirectX
 *   PS2 CDROM (filesystem)         → NTFS
 *   MIPS R5900                     → x86_64
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

/* Internal headers */
#include "sys/win32.h"
#include "input/gamepad.h"
#include "render/opengl.h"
#include "audio/openal.h"
#include "game/player.h"
#include "game/camera.h"
#include "game/world.h"
#include "game/scene.h"

/* ============================================
 * Глобальное состояние игры
 * ============================================ */

typedef struct {
    int running;
    int fullscreen;
    int window_width;
    int window_height;
    int fps;
    int frame_count;
    double last_fps_time;
    
    /* Systems */
    Win32State* win32;
    OpenGLRenderer* gl;
    GamepadState* input;
    OpenALAudio* audio;
    PlayerData* player;
    CameraData* camera;
    WorldData* world;
    SceneData* scene;
} GameState;

static GameState g_game;

/* ============================================
 * Инициализация
 * ============================================ */

int Game_Init(void) {
    printf("=== DMC1 PC Port ===\n");
    printf("Initializing subsystems...\n");
    
    /* Win32 window */
    g_game.win32 = Win32_Create("Devil May Cry", 1280, 720, 32);
    if (!g_game.win32) {
        printf("ERROR: Failed to create window\n");
        return 0;
    }
    
    /* OpenGL context */
    g_game.gl = OpenGL_Create((void*)g_game.win32->hwnd);
    if (!g_game.gl) {
        printf("ERROR: Failed to create OpenGL context\n");
        MessageBoxA(NULL, "Failed to create OpenGL context.\n\nMake sure your GPU drivers are installed.", 
                    "Graphics Error", MB_ICONERROR);
        return 0;
    }
    
    /* Input system (WASD + mouse) */
    g_game.input = Gamepad_Init();
    if (!g_game.input) {
        printf("ERROR: Failed to initialize input\n");
        return 0;
    }
    
    /* Audio system */
    g_game.audio = OpenAL_Create();
    if (!g_game.audio) {
        printf("WARNING: Audio initialization failed (continuing without sound)\n");
    }
    
    /* Game systems */
    g_game.player = Player_Create();
    g_game.camera = Camera_Create();
    g_game.world = World_Create();
    g_game.scene = Scene_Create();
    
    if (!g_game.player || !g_game.camera || !g_game.world || !g_game.scene) {
        printf("ERROR: Failed to create game systems\n");
        return 0;
    }
    
    /* Load game data from disk */
    /* TODO: Load level data from PS2 FSD files */
    /* Start in game scene — controls screen handled in scene.c */
    
    printf("All systems initialized successfully!\n");
    printf("Controls:\n");
    printf("  WASD - Movement\n");
    printf("  LMB - Attack\n");
    printf("  Mouse - Camera\n");
    printf("  Space - Jump\n");
    printf("  Shift - Dodge\n");
    printf("  R - Block\n");
    printf("  Q/E - Weapon slots\n");
    printf("  Enter - Menu\n");
    printf("\n");
    
    return 1;
}

/* ============================================
 * Обновление кадра
 * ============================================ */

void Game_UpdateFrame(double dt) {
    /* Update input */
    Gamepad_Update(g_game.input, g_game.win32);
    
    /* Update game systems */
    Player_Update(g_game.player, g_game.input, dt);
    Camera_Update(g_game.camera, g_game.player, g_game.input, dt);
    World_Update(g_game.world, dt);
    Scene_Update(g_game.scene, dt);
}

/* ============================================
 * Рендеринг кадра
 * ============================================ */

void Game_RenderFrame(void) {
    /* Clear */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    /* Setup camera */
    Camera_SetupOpenGL(g_game.camera);
    
    /* Render world */
    World_Render(g_game.world);
    
    /* Render player */
    Player_Render(g_game.player);
    
    /* Render UI */
    Scene_RenderUI(g_game.scene);
    
    /* Swap buffers */
    Win32_SwapBuffers(g_game.win32);
}

/* ============================================
 * Главный игровой цикл
 * ============================================ */

void Game_MainLoop(void) {
    double last_time = 0;
    double current_time = 0;
    
    g_game.running = 1;
    g_game.frame_count = 0;
    g_game.last_fps_time = Win32_GetTime();
    
    printf("Starting main loop...\n");
    
    while (g_game.running) {
        /* Process Windows messages */
        Win32_ProcessMessages();
        
        /* Check for quit */
        if (!Win32_IsActive(g_game.win32)) {
            break;
        }
        
        /* Calculate delta time */
        current_time = Win32_GetTime();
        double dt = current_time - last_time;
        if (dt > 0.05) dt = 0.05; /* Cap at 50ms */
        last_time = current_time;
        
        /* Update game */
        Game_UpdateFrame(dt);
        
        /* Render */
        Game_RenderFrame();
        
        /* FPS counter */
        g_game.frame_count++;
        if (current_time - g_game.last_fps_time >= 1.0) {
            g_game.fps = g_game.frame_count;
            g_game.frame_count = 0;
            g_game.last_fps_time = current_time;
        }
    }
}

/* ============================================
 * Очистка
 * ============================================ */

void Game_Shutdown(void) {
    printf("Shutting down...\n");
    
    if (g_game.scene) Scene_Destroy(g_game.scene);
    if (g_game.world) World_Destroy(g_game.world);
    if (g_game.camera) Camera_Destroy(g_game.camera);
    if (g_game.player) Player_Destroy(g_game.player);
    if (g_game.audio) OpenAL_Destroy(g_game.audio);
    if (g_game.input) Gamepad_Destroy(g_game.input);
    if (g_game.gl) OpenGL_Destroy(g_game.gl);
    if (g_game.win32) Win32_Destroy(g_game.win32);
    
    printf("Shutdown complete.\n");
}

/* ============================================
 * WinMain - Точка входа Windows
 * ============================================ */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    /* Initialize subsystems */
    if (!Game_Init()) {
        MessageBoxA(NULL, "Failed to initialize DMC1 PC Port", 
                    "Error", MB_ICONERROR);
        return 1;
    }
    
    /* Run main loop */
    Game_MainLoop();
    
    /* Cleanup */
    Game_Shutdown();
    
    return 0;
}
