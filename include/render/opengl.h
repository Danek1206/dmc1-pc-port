/*
 * DMC1 PC Port - OpenGL Renderer
 * Замена PS2 GS (Graphical Synthesizer)
 * 
 * Поддерживает:
 *   - 3D рендеринг с текстурами
 *   - Блендинг (для прозрачности)
 *   - Z-buffer
 *   - Освещение
 *   - Шейдеры (программируемый пайплайн)
 */

#ifndef DMC1_OPENGL_H
#define DMC1_OPENGL_H

#include <stddef.h>

typedef struct {
    void*    context;
    void*    hdc;       /* Device context */
    int      width;
    int      height;
    int      initialized;
} OpenGLRenderer;

/* Create OpenGL renderer */
OpenGLRenderer* OpenGL_Create(void* window_handle);

/* Set render target */
void OpenGL_SetViewport(OpenGLRenderer* renderer, int w, int h);

/* Clear screen */
void OpenGL_Clear(OpenGLRenderer* renderer, float r, float g, float b, float a);

/* Enable/disable features */
void OpenGL_EnableDepthTest(OpenGLRenderer* renderer, int enable);
void OpenGL_EnableBlending(OpenGLRenderer* renderer, int enable);
void OpenGL_EnableCullFace(OpenGLRenderer* renderer, int enable);

/* Draw primitives */
void OpenGL_DrawTriangles(OpenGLRenderer* renderer, 
                          void* vertices, int count, void* indices, int index_count);

/* Texture management */
int  OpenGL_CreateTexture(OpenGLRenderer* renderer, 
                          const unsigned char* data, int w, int h);
void OpenGL_DeleteTexture(OpenGLRenderer* renderer, int tex_id);
void OpenGL_BindTexture(OpenGLRenderer* renderer, int tex_id);

/* Destroy renderer */
void OpenGL_Destroy(OpenGLRenderer* renderer);

#endif /* DMC1_OPENGL_H */
