/*
 * dmc_viewer - interactive texture browser for DMC1 disc data.
 *
 * Окно со списком .TM2 с диска: листание стрелками, Home/End, Esc - выход.
 * Это первая «вещественная» проверка загрузчика TIM2: если картинка
 * совпадает с игрой - формат закрыт.
 *
 * Usage:
 *   dmc_viewer [ROOT_DIR]     (default E:\DATA)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <GL/gl.h>

#include "loader/tim2.h"
#include "loader/t32.h"

#define MAX_FILES 512

static char  g_files[MAX_FILES][260];
static int   g_nfiles = 0, g_cur = 0;

static int   g_w = 1024, g_h = 768;
static GLuint g_tex = 0;
static uint32_t g_tw = 0, g_th = 0;
static char  g_status[512] = "";
static char  g_glinfo[256] = "";
static int   g_probe_id = 0, g_probe_w = 0, g_probe_h = 0;
static char  g_center[64] = "";

static uint8_t *g_last_rgba = NULL;

/* ------------------------------------------------------------- file scan */

static void add_file(const char *path)
{
    if (g_nfiles < MAX_FILES) {
        strncpy(g_files[g_nfiles], path, 259);
        g_files[g_nfiles][259] = 0;
        g_nfiles++;
    }
}

static void scan_dir(const char *dir)
{
    char pattern[300];
    char sub[300];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    _snprintf(pattern, sizeof(pattern) - 1, "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", dir, fd.cFileName);
            scan_dir(sub);
        } else {
            size_t n = strlen(fd.cFileName);
            /*
             * На диске лежат TM2 нулевого размера (RLT_F_F/G/I/S) - это
             * заглушки под вырезанный контент, показывать их нет смысла.
             */
            if (fd.nFileSizeLow > 0 &&
                (n > 4 && (!_stricmp(fd.cFileName + n - 4, ".TM2") ||
                           !(_stricmp(fd.cFileName + n - 4, ".T32"))))) {
                char full[300];
                _snprintf(full, sizeof(full) - 1, "%s\\%s", dir, fd.cFileName);
                add_file(full);
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* ------------------------------------------------------------- GL loader */

/*
 * Все используемые функции входят в OpenGL 1.1, который экспортируется
 * opengl32.lib напрямую. Динамически через wglGetProcAddress их брать
 * нельзя: для функций 1.1 этот путь возвращает неприменимый к текущему
 * контексту указатель, загрузка текстуры не выполняется и вместо
 * изображения рисуется белый прямоугольник цвета примитива.
 */

/* ---------------------------------------------------------- texture load */

static const char *basename_of(const char *p)
{
    const char *s = strrchr(p, '\\');
    return s ? s + 1 : p;
}

static void load_current(void)
{
    TIM2Texture *t = NULL;
    T32Texture *a = NULL;
    uint8_t *rgba = NULL;
    GLenum err;

    if (g_nfiles == 0)
        return;
    if (g_cur < 0) g_cur = 0;
    if (g_cur >= g_nfiles) g_cur = g_nfiles - 1;

    /*
     * Тип файла определяется содержимым, а не расширением: TIM2 начинается
     * с 'TIM2', всё остальное пробуются как T32.
     */
    t = TIM2_LoadFile(g_files[g_cur]);
    if (t) {
        if (!TIM2_DecodeAlloc(t, &rgba)) {
            TIM2_Free(t);
            t = NULL;
        }
    }
    if (!t) {
        a = T32_LoadFile(g_files[g_cur]);
        if (!a || !T32_DecodeAlloc(a, &rgba)) {
            if (a)
                T32_Free(a);
            _snprintf(g_status, sizeof(g_status) - 1, "%s  -  NOT TIM2/T32",
                      basename_of(g_files[g_cur]));
            return;
        }
    }

    free(g_last_rgba);
    g_last_rgba = rgba;
    g_tw = t ? t->width : a->width;
    g_th = t ? t->height : a->height;

    if (!g_tex)
        glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)g_tw, (GLsizei)g_th, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    err = glGetError();

    /*
     * Проверка, что данные легли в текстуру: читаем обратно пиксель из
     * середины. Если получится 0,0,0,0 - загрузка не состоялась.
     */
    {
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &g_probe_w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &g_probe_h);
        g_probe_id = (int)g_tex;
    }

    _snprintf(g_status, sizeof(g_status) - 1,
              "%s   %ux%u  %s  [%d/%d]  tex=%d glerr=%04X",
              basename_of(g_files[g_cur]), g_tw, g_th,
              t ? "TIM2" : "T32 8bpp",
              g_cur + 1, g_nfiles, g_probe_id, (unsigned)err);
    if (t)
        TIM2_Free(t);
    if (a)
        T32_Free(a);
}

/* ------------------------------------------------------------- window */

static HGLRC g_ctx = NULL;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        if (wp == VK_RIGHT || wp == VK_DOWN) { g_cur++; load_current(); }
        if (wp == VK_LEFT  || wp == VK_UP)   { g_cur--; load_current(); }
        if (wp == VK_HOME) { g_cur = 0; load_current(); }
        if (wp == VK_END)  { g_cur = g_nfiles - 1; load_current(); }
        return 0;
    case WM_SIZE:
        g_w = LOWORD(lp);
        g_h = HIWORD(lp);
        if (g_h < 1) g_h = 1;
        glViewport(0, 0, g_w, g_h);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/*
 * Кэш ассетов ищется рядом с exe, а не по текущей папке: запуск двойным
 * щелчком ставит рабочей папкой ту, откуда запущен процесс, и относительный
 * путь "assets" тогда не находит ничего.
 */
static void default_root(char *out, size_t n)
{
    char exe[MAX_PATH];
    char *slash;

    out[0] = 0;
    if (!GetModuleFileNameA(NULL, exe, sizeof(exe)))
        return;
    slash = strrchr(exe, '\\');
    if (slash)
        *slash = 0;
    /* exe лежит в build\, кэш - в корне проекта */
    _snprintf(out, n - 1, "%s\\assets", exe);
    if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES)
        _snprintf(out, n - 1, "%s\\..\\assets", exe);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    HDC dc;
    PIXELFORMATDESCRIPTOR pfd;
    MSG m;
    static char rootbuf[MAX_PATH];
    const char *root;

    (void)prev;

    if (cmd && *cmd) {
        /*
         * Windows передаёт путь с пробелами (и путь из drag-drop на exe)
         * в двойных кавычках. LPSTR cmd сохраняет их буквально, поэтому
         * обрамляющие кавычки снимаются, иначе GetFileAttributesA и
         * FindFirstFileA видят кавычки как часть имени.
         */
        size_t clen = strlen(cmd);
        if (clen >= 2 && cmd[0] == '"' && cmd[clen - 1] == '"') {
            static char unquoted[MAX_PATH * 3];
            size_t inner = clen - 2;
            if (inner < sizeof(unquoted)) {
                memcpy(unquoted, cmd + 1, inner);
                unquoted[inner] = 0;
                cmd = unquoted;
            }
        }
        root = cmd;
    } else {
        default_root(rootbuf, sizeof(rootbuf));
        root = rootbuf;
    }

    /*
     * Аргументом может быть отдельный файл (точечная проверка) или каталог.
     * GetFileAttributesA возвращает валидные атрибуты и для каталога, поэтому
     * различает их признак FILE_ATTRIBUTE_DIRECTORY.
     */
    {
        DWORD attr = GetFileAttributesA(root);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            add_file(root);
        else
            scan_dir(root);
    }
    if (g_nfiles == 0) {
        char full[MAX_PATH * 2];
        char buf[MAX_PATH * 3];
        DWORD n = GetFullPathNameA(root, sizeof(full), full, NULL);
        if (!n || n > sizeof(full) - 1)
            strcpy(full, root);
        _snprintf(buf, sizeof(buf) - 1,
                  "No .TM2 or .T32 files found under\n\n%s", full);
        MessageBoxA(NULL, buf, "dmc_viewer", MB_OK | MB_ICONERROR);
        return 1;
    }
    /* сортировка по имени, чтобы листание было предсказуемым */
    {
        int i, j;
        for (i = 1; i < g_nfiles; ++i) {
            char tmp[260];
            strcpy(tmp, g_files[i]);
            for (j = i; j > 0 && _stricmp(g_files[j - 1], tmp) > 0; --j)
                strcpy(g_files[j], g_files[j - 1]);
            strcpy(g_files[j], tmp);
        }
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "dmc_viewer";
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "dmc_viewer", "DMC1 texture viewer",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           g_w, g_h, NULL, NULL, inst, NULL);
    if (!hwnd)
        return 1;

    dc = GetDC(hwnd);
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    g_ctx = wglCreateContext(dc);
    if (!g_ctx) {
        MessageBoxA(NULL, "no OpenGL context", "dmc_viewer", MB_OK);
        return 1;
    }
    wglMakeCurrent(dc, g_ctx);
    glViewport(0, 0, g_w, g_h);
    {
        const char *v = (const char *)glGetString(GL_VERSION);
        const char *n = (const char *)glGetString(GL_VENDOR);
        _snprintf(g_glinfo, sizeof(g_glinfo) - 1, "%s / %s",
                  v ? v : "?", n ? n : "?");
    }

    load_current();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageA(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);

        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (g_tex && g_tw && g_th) {
            /*
             * Вписывание с сохранением пропорций. В нормализованных координатах
             * [-1,1]x[-1,1] за горизонталь отвечает wa, поэтому реальный aspect
             * рисуемого прямоугольника = w*wa/h. Приравнивая его aspect
             * текстуры, получаем w/h = aspect/wa; ограничиваем большую ось 1.
             * Ранее ветки были перепутаны, из-за чего узкие текстуры
             * растягивались на всю ширину окна.
             */
            float aspect = (float)g_tw / (float)g_th;
            float wa = (float)g_w / (float)g_h;
            float w = 1.0f, h = 1.0f;
            if (aspect >= wa) { w = 1.0f; h = wa / aspect; }
            else              { h = 1.0f; w = aspect / wa; }
            glEnable(GL_TEXTURE_2D);
            /*
             * У текстур интерфейса DMC прозрачный фон в альфа-канале.
             * Без смешивания такие пиксели показывают свой RGB (часто белый)
             * и вся картинка выглядит белым прямоугольником.
             */
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindTexture(GL_TEXTURE_2D, g_tex);
            glColor3f(1, 1, 1);
            glBegin(GL_QUADS);
            /*
             * Растры TIM2 хранят строки сверху вниз (y=0 - верхняя строка),
             * а в OpenGL v=0 соответствует нижнему краю текстуры. Поэтому
             * верхнему краю квадрата соответствует v=0.
             */
            glTexCoord2f(0, 0); glVertex2f(-w,  h);
            glTexCoord2f(1, 0); glVertex2f( w,  h);
            glTexCoord2f(1, 1); glVertex2f( w, -h);
            glTexCoord2f(0, 1); glVertex2f(-w, -h);
            glEnd();
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
        }
        /*
         * Числовая самопроверка: читаем пиксель из центра кадра. Белый
         * 255,255,255 означает, что примитив отрисован без текстуры.
         */
        {
            uint8_t c[4] = {0, 0, 0, 0};
            glReadPixels(g_w / 2, g_h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
            _snprintf(g_center, sizeof(g_center) - 1, "center=%d,%d,%d,%d",
                      c[0], c[1], c[2], c[3]);
        }
        SwapBuffers(dc);

        {
            char title[600];
            _snprintf(title, sizeof(title) - 1, "dmc_viewer - %s | %s | %s",
                      g_status, g_center, g_glinfo);
            SetWindowTextA(hwnd, title);
        }
    }

    free(g_last_rgba);
    return 0;
}
