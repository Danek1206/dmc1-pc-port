/*
 * dmc_tune - интерактивный подбор раскладки T32 глазами пользователя.
 *
 * Ключевая идея: формулу и ширину нельзя вычислить по одним байтам без
 * эталона. Поэтому перебор выводится на экран, а решение принимает человек.
 *
 * Управление:
 *   A / D        - переключить кандидата раскладки (ширина x режим)
 *   W / S        - следующий / предыдущий файл в папке
 *   Esc          - выход (в заголовке всегда текущая ширина, высота, режим)
 *
 * Заголовок окна показывает параметры выбранного кандидата. Когда картинка
 * станет узнаваемой, эти параметры вшиваются в src/loader/t32.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <GL/gl.h>

#define CLUT 0x400
#define MAXF 1024

static char g_files[MAXF][260];
static int  g_nf = 0, g_cur = 0;
static int  g_cand = 0;

/* кандидаты: (width, mode) mode: 0=linear 1=canon 2=table */
typedef struct { int w, h, mode; } Cand;
static Cand g_cands[64];
static int  g_ncand = 0;

static int   g_w = 1024, g_h = 768;
static GLuint g_tex = 0;
static char  g_status[512] = "";

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* табличная раскладка (старая) */
static int g_table[128 * 64];
static const int PG[32] = {0,1,4,5,16,17,20,21,2,3,6,7,18,19,22,23,
                           8,9,12,13,24,25,28,29,10,11,14,15,26,27,30,31};
static const int CS[128] = {
     0, 4,16,20,32,36,48,52, 2, 6,18,22,34,38,50,54,
     8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62,
    33,37,49,53, 1, 5,17,21,35,39,51,55, 3, 7,19,23,
    41,45,57,61, 9,13,25,29,43,47,59,63,11,15,27,31,
    32,36,48,52, 0, 4,16,20,34,38,50,54, 2, 6,18,22,
    40,44,56,60, 8,12,24,28,42,46,58,62,10,14,26,30,
     1, 5,17,21,33,37,49,53, 3, 7,19,23,35,39,51,55,
     9,13,25,29,41,45,57,61,11,15,27,31,43,47,59,63};
static void build_table(void)
{
    int b, i;
    for (b = 0; b < 32; ++b) {
        int base = PG[b] * 256, bx = (b % 8) * 16, by = (b / 8) * 16;
        for (i = 0; i < 256; ++i) {
            int pos = base + CS[i % 128] + (i / 64) * 64;
            int x = bx + (i % 16), y = by + (i / 16);
            if (x < 128 && y < 64) g_table[y * 128 + x] = pos;
        }
    }
}

static void deswizzle(int mode, const uint8_t *src, uint8_t *dst,
                      int width, int height)
{
    size_t n = (size_t)width * height;
    int x, y;
    if (mode == 0) { memcpy(dst, src, n); return; }
    if (mode == 1) { /* канон unswizzle8_ps2 */
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x) {
                int bl = (y & ~0xf) * width + (x & ~0xf) * 2;
                int sw = (((y + 2) >> 2) & 1) * 4;
                int py = (((y & ~3) >> 1) + (y & 1)) & 7;
                int cl = py * width * 2 + ((x + sw) & 7) * 4;
                int bn = ((y >> 1) & 1) + ((x >> 2) & 2);
                size_t si = (size_t)bl + cl + bn;
                if (si < n) dst[y * width + x] = src[si];
            }
        return;
    }
    /* mode==2: таблица страниц 128x64 */
    {
        int ppr = width / 128;
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x) {
                int page = (y / 64) * ppr + (x / 128);
                int pos = (y % 64) * 128 + (x % 128);
                size_t si = (size_t)page * 8192 + g_table[pos];
                if (si < n) dst[y * width + x] = src[si];
            }
    }
}

static void scan_dir(const char *dir)
{
    char pat[300], sub[300], full[300];
    WIN32_FIND_DATAA fd; HANDLE h;
    _snprintf(pat, sizeof(pat) - 1, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        size_t n;
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", dir, fd.cFileName);
            scan_dir(sub);
        } else {
            n = strlen(fd.cFileName);
            if (fd.nFileSizeLow > 0 && n > 4 &&
                !_stricmp(fd.cFileName + n - 4, ".T32")) {
                if (g_nf < MAXF) {
                    _snprintf(full, sizeof(full) - 1, "%s\\%s", dir, fd.cFileName);
                    strncpy(g_files[g_nf], full, 259);
                    g_files[g_nf][259] = 0; g_nf++;
                }
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static const char *base_of(const char *p)
{ const char *s = strrchr(p, '\\'); return s ? s + 1 : p; }

static void build_cands(uint32_t raster)
{
    uint32_t w;
    g_ncand = 0;
    for (w = 128; w <= raster / 64 && g_ncand < 60; w += 128) {
        if (raster % w) continue;
        if ((raster / w) % 64) continue;
        g_cands[g_ncand].w = (int)w; g_cands[g_ncand].h = (int)(raster / w);
        g_cands[g_ncand].mode = 1; g_ncand++;
    }
    /* + линейные и табличные для известной высоты 256 */
    { uint32_t kw[6] = {128,256,448,512,768,896}; int i;
      for (i = 0; i < 6 && g_ncand < 60; ++i) {
          if (raster % kw[i]) continue;
          g_cands[g_ncand].w = (int)kw[i]; g_cands[g_ncand].h = (int)(raster / kw[i]);
          g_cands[g_ncand].mode = 0; g_ncand++;
          g_cands[g_ncand].w = (int)kw[i]; g_cands[g_ncand].h = (int)(raster / kw[i]);
          g_cands[g_ncand].mode = 2; g_ncand++;
      }
    }
    if (g_cand >= g_ncand) g_cand = 0;
}

static uint8_t *g_rgba = NULL;
static int g_tw = 0, g_th = 0;

static void load_current(void)
{
    FILE *fp; long size; uint8_t *buf; uint8_t *lin;
    uint32_t raster, payload, roff;
    int i;
    if (!g_nf) return;
    if (g_cur < 0) g_cur = 0; if (g_cur >= g_nf) g_cur = g_nf - 1;

    fp = fopen(g_files[g_cur], "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) { fclose(fp); return; }
    fclose(fp);

    raster  = (rd32(buf + 0x04) >> 16) << 8;
    payload = rd32(buf + 0x0C);
    roff    = (uint32_t)(size - payload - CLUT);
    build_cands(raster);
    if (g_cand >= g_ncand) g_cand = 0;

    {
        Cand c = g_cands[g_cand];
        size_t np = (size_t)c.w * c.h;
        const uint8_t *clut = buf + (size - CLUT);
        free(g_rgba);
        g_rgba = (uint8_t *)malloc(np * 4);
        lin = (uint8_t *)malloc(np);
        deswizzle(c.mode, buf + roff, lin, c.w, c.h);
        for (i = 0; i < (int)np; ++i) {
            const uint8_t *e = clut + (size_t)lin[i] * 4u;
            uint8_t *d = g_rgba + i * 4u;
            d[0]=e[0]; d[1]=e[1]; d[2]=e[2]; d[3]=e[3];
        }
        free(lin);
        g_tw = c.w; g_th = c.h;

        if (!g_tex) glGenTextures(1, &g_tex);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c.w, c.h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_rgba);
        _snprintf(g_status, sizeof(g_status) - 1,
                  "%s  W=%d H=%d  MODE=%s  [cand %d/%d]  [file %d/%d]",
                  base_of(g_files[g_cur]), c.w, c.h,
                  c.mode==0?"linear":(c.mode==1?"canon":"table"),
                  g_cand + 1, g_ncand, g_cur + 1, g_nf);
    }
    free(buf);
}

static LRESULT CALLBACK wnd_proc(HWND hw, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CLOSE: case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        if (wp == 'D' || wp == VK_RIGHT) { g_cand = (g_cand + 1) % (g_ncand?g_ncand:1); load_current(); }
        if (wp == 'A' || wp == VK_LEFT)  { g_cand = (g_cand + g_ncand - 1) % (g_ncand?g_ncand:1); load_current(); }
        if (wp == 'W' || wp == VK_UP)    { g_cur--; g_cand = 0; load_current(); }
        if (wp == 'S' || wp == VK_DOWN)  { g_cur++; g_cand = 0; load_current(); }
        return 0;
    case WM_SIZE:
        g_w = LOWORD(lp); g_h = HIWORD(lp); if (g_h < 1) g_h = 1;
        glViewport(0, 0, g_w, g_h); return 0;
    }
    return DefWindowProcA(hw, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc; HWND hw; HDC dc; PIXELFORMATDESCRIPTOR pfd; MSG msg;
    (void)prev;
    build_table();
    if (cmd && *cmd) {
        size_t c = strlen(cmd); DWORD attr;
        if (c >= 2 && cmd[0]=='"' && cmd[c-1]=='"') { memmove(cmd, cmd+1, c-2); cmd[c-2]=0; }
        attr = GetFileAttributesA(cmd);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            strncpy(g_files[0], cmd, 259); g_nf = 1;
        } else scan_dir(cmd);
    }
    if (!g_nf) { MessageBoxA(NULL, "no .T32", "dmc_tune", MB_OK); return 1; }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc; wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.lpszClassName = "dmc_tune";
    RegisterClassA(&wc);
    hw = CreateWindowExA(0, "dmc_tune", "dmc_tune", WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT, g_w, g_h, NULL, NULL, inst, NULL);
    if (!hw) return 1;
    dc = GetDC(hw);
    memset(&pfd, 0, sizeof(pfd)); pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    if (!wglMakeCurrent(dc, wglCreateContext(dc))) return 1;
    glViewport(0, 0, g_w, g_h);
    load_current();
    ShowWindow(hw, show);

    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); DispatchMessage(&msg);
        glClearColor(0.10f,0.10f,0.12f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (g_tex && g_tw && g_th) {
            float a = (float)g_tw / (float)g_th, wa = (float)g_w / (float)g_h;
            float w = 1, h = 1;
            if (a >= wa) { w = 1; h = wa / a; } else { h = 1; w = a / wa; }
            glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindTexture(GL_TEXTURE_2D, g_tex); glColor3f(1,1,1);
            glBegin(GL_QUADS);
            glTexCoord2f(0,0); glVertex2f(-w, h);
            glTexCoord2f(1,0); glVertex2f( w, h);
            glTexCoord2f(1,1); glVertex2f( w,-h);
            glTexCoord2f(0,1); glVertex2f(-w,-h);
            glEnd();
            glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D);
        }
        SwapBuffers(dc);
        { char t[600]; _snprintf(t, sizeof(t)-1, "dmc_tune | %s | A/D layout W/S file", g_status);
          SetWindowTextA(hw, t); }
    }
    free(g_rgba);
    return 0;
}
