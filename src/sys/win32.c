/*
 * DMC1 PC Port - Win32 Window System
 * Создание и управление окном Windows
 */

#include "sys/win32.h"
#include <windows.h>
#include <stdlib.h>

/* ============================================
 * Внутренние функции окна
 * ============================================ */

static LRESULT CALLBACK Win32_WindowProc(HWND hwnd, UINT msg, 
                                          WPARAM wParam, LPARAM lParam) {
    Win32State* state = NULL;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        state = (Win32State*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
    } else {
        state = (Win32State*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    switch (msg) {
        case WM_SIZE:
            if (state) {
                state->width = LOWORD(lParam);
                state->height = HIWORD(lParam);
            }
            return 0;
            
        case WM_CLOSE:
            if (state) state->active = 0;
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_MOUSEMOVE:
            if (state) {
                state->mouse_x = (short)LOWORD(lParam);
                state->mouse_y = (short)HIWORD(lParam);
            }
            return 0;
            
        case WM_LBUTTONDOWN:
            if (state) state->mouse_buttons |= MOUSE_LEFT;
            return 0;
            
        case WM_LBUTTONUP:
            if (state) state->mouse_buttons &= ~MOUSE_LEFT;
            return 0;
            
        case WM_RBUTTONDOWN:
            if (state) state->mouse_buttons |= MOUSE_RIGHT;
            return 0;
            
        case WM_RBUTTONUP:
            if (state) state->mouse_buttons &= ~MOUSE_RIGHT;
            return 0;
            
        case WM_KEYDOWN:
            if (state) state->keys[wParam] = 1;
            return 0;
            
        case WM_KEYUP:
            if (state) state->keys[wParam] = 0;
            return 0;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

/* ============================================
 * Создание окна
 * ============================================ */

Win32State* Win32_Create(const char* title, int width, int height, int bpp) {
    Win32State* state = (Win32State*)calloc(1, sizeof(Win32State));
    if (!state) return NULL;
    
    state->width = width;
    state->height = height;
    state->bpp = bpp;
    state->active = 1;
    
    /* Register window class */
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32_WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "DMC1_PC_PORT";
    
    if (!RegisterClassA(&wc)) {
        free(state);
        return NULL;
    }
    
    /* Calculate window position */
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    
    /* Create window */
    state->hwnd = CreateWindowA(
        "DMC1_PC_PORT", title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, GetModuleHandle(NULL), state
    );
    
    if (!state->hwnd) {
        free(state);
        return NULL;
    }
    
    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    
    return state;
}

/* ============================================
 * Управление окном
 * ============================================ */

void Win32_ProcessMessages(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int Win32_IsActive(Win32State* state) {
    return state ? state->active : 0;
}

void Win32_Destroy(Win32State* state) {
    if (!state) return;
    if (state->hwnd) DestroyWindow(state->hwnd);
    UnregisterClassA("DMC1_PC_PORT", GetModuleHandle(NULL));
    free(state);
}

double Win32_GetTime(void) {
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}

void Win32_SwapBuffers(Win32State* state) {
    if (!state || !state->hwnd) return;
    HDC hdc = GetDC(state->hwnd);
    if (hdc) {
        SwapBuffers(hdc);
        ReleaseDC(state->hwnd, hdc);
    }
}
