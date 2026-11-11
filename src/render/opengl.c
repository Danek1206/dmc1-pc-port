/*
 * DMC1 PC Port - OpenGL Renderer Implementation
 * Замена PS2 Graphical Synthesizer
 */

#include "render/opengl.h"
#include "sys/win32.h"
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>

/* Fallback for older OpenGL headers */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

/* ============================================
 * Создание OpenGL контекста
 * ============================================ */

OpenGLRenderer* OpenGL_Create(void* window_handle) {
    OpenGLRenderer* renderer = (OpenGLRenderer*)calloc(1, sizeof(OpenGLRenderer));
    if (!renderer) return NULL;
    
    HWND hwnd = (HWND)window_handle;
    
    /* Get device context */
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        free(renderer);
        return NULL;
    }
    
    /* Set pixel format */
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf) {
        ReleaseDC(hwnd, hdc);
        free(renderer);
        return NULL;
    }
    
    SetPixelFormat(hdc, pf, &pfd);
    
    /* Create OpenGL context */
    renderer->context = (void*)wglCreateContext(hdc);
    if (!renderer->context) {
        ReleaseDC(hwnd, hdc);
        free(renderer);
        return NULL;
    }
    
    wglMakeCurrent(hdc, renderer->context);
    
    /* Store HDC for later use */
    renderer->hdc = (void*)hdc;
    renderer->width = 1280;
    renderer->height = 720;
    renderer->initialized = 1;
    
    /* Enable OpenGL features */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_NORMALIZE);
    
    /* Set clear color (dark blue sky) */
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    
    return renderer;
}

/* ============================================
 * Управление рендерингом
 * ============================================ */

void OpenGL_SetViewport(OpenGLRenderer* renderer, int w, int h) {
    if (!renderer) return;
    renderer->width = w;
    renderer->height = h;
    glViewport(0, 0, w, h);
}

void OpenGL_Clear(OpenGLRenderer* renderer, float r, float g, float b, float a) {
    (void)renderer;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGL_EnableDepthTest(OpenGLRenderer* renderer, int enable) {
    (void)renderer;
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void OpenGL_EnableBlending(OpenGLRenderer* renderer, int enable) {
    (void)renderer;
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void OpenGL_EnableCullFace(OpenGLRenderer* renderer, int enable) {
    (void)renderer;
    if (enable) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
}

/* ============================================
 * Рисование
 * ============================================ */

void OpenGL_DrawTriangles(OpenGLRenderer* renderer, 
                          void* vertices, int count, 
                          void* indices, int index_count) {
    (void)renderer;
    
    if (!vertices || count == 0) return;
    
    /* Simple triangle rendering (will be expanded with proper vertex buffers) */
    if (indices && index_count > 0) {
        /* Indexed drawing */
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, indices);
    } else {
        /* Non-indexed drawing */
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
}

/* ============================================
 * Текстуры
 * ============================================ */

int OpenGL_CreateTexture(OpenGLRenderer* renderer, 
                          const unsigned char* data, int w, int h) {
    (void)renderer;
    
    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return (int)tex_id;
}

void OpenGL_DeleteTexture(OpenGLRenderer* renderer, int tex_id) {
    (void)renderer;
    if (tex_id > 0) {
        glDeleteTextures(1, (const GLuint*)&tex_id);
    }
}

void OpenGL_BindTexture(OpenGLRenderer* renderer, int tex_id) {
    (void)renderer;
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex_id);
}

/* ============================================
 * Уничтожение
 * ============================================ */

void OpenGL_Destroy(OpenGLRenderer* renderer) {
    if (!renderer) return;
    
    if (renderer->context) {
        /* Get HDC from the renderer's context to unmake current */
        HDC hdc = (HDC)renderer->hdc;
        wglMakeCurrent(hdc, NULL);
        wglDeleteContext((HGLRC)renderer->context);
    }
    
    free(renderer);
}
