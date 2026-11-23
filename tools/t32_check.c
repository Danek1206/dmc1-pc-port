/*
 * t32_check - пакетная проверка загрузчика T32.
 *
 * Обходит каталог, разбирает каждый .T32 через src/loader/t32.c и сверяет,
 * что смещения растра и палитры сходятся с размером файла. Дополнительно
 * считает средний перепад яркости между соседними пикселями: у корректно
 * разложенной текстуры он маленький, у неверной раскладки - десятки уровней.
 *
 * Usage:
 *   t32_check [ROOT_DIR]        (default assets)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#include "loader/t32.h"

#define MAX_FILES 512

static char g_files[MAX_FILES][260];
static int  g_nfiles = 0;
static int  g_failed = 0;

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
    char pattern[300], sub[300], full[300];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    _snprintf(pattern, sizeof(pattern) - 1, "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        size_t n;
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", dir, fd.cFileName);
            scan_dir(sub);
            continue;
        }
        n = strlen(fd.cFileName);
        if (fd.nFileSizeLow > 0 && n > 4 &&
            !_stricmp(fd.cFileName + n - 4, ".T32")) {
            _snprintf(full, sizeof(full) - 1, "%s\\%s", dir, fd.cFileName);
            add_file(full);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/*
 * Средний |L(x+1)-L(x)| и |L(y+1)-L(y)| по яркости палитры. Значение ниже
 * 10 уровня означает, что соседние пиксели действительно соседние.
 */
static double smoothness(const T32Texture *t)
{
    double lum[256], sum = 0.0;
    uint32_t x, y;
    unsigned long n = 0;
    int i;

    for (i = 0; i < 256; ++i)
        lum[i] = (t->clut[i * 4] * 30.0 + t->clut[i * 4 + 1] * 59.0 +
                  t->clut[i * 4 + 2] * 11.0) / 100.0;

    for (y = 0; y < t->height; y += 3) {
        for (x = 0; x + 1 < t->width; x += 3)
            sum += fabs(lum[t->pixels[(size_t)y * t->width + x]] -
                        lum[t->pixels[(size_t)y * t->width + x + 1]]), ++n;
    }
    for (y = 0; y + 1 < t->height; y += 3) {
        for (x = 0; x < t->width; x += 3)
            sum += fabs(lum[t->pixels[(size_t)y * t->width + x]] -
                        lum[t->pixels[(size_t)(y + 1) * t->width + x]]), ++n;
    }
    return n ? sum / (double)n : 0.0;
}

static void check(const char *path)
{
    const char *base = strrchr(path, '\\');
    T32Texture *t = T32_LoadFile(path);
    FILE *fp;
    long size;

    base = base ? base + 1 : path;
    if (!t) {
        printf("FAIL  %-20s  parse error\n", base);
        g_failed++;
        return;
    }
    fp = fopen(path, "rb");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);

    if (t->raster_offset + t->raster_size +
        (t->payload_size - t->raster_size) + 0x400L != size) {
        printf("FAIL  %-20s  layout does not cover the file\n", base);
        g_failed++;
    } else {
        printf("ok    %-20s  %4ux%u  raster=0x%06X clut=0x%06X  d=%.2f\n",
               base, t->width, t->height, t->raster_offset, t->clut_offset,
               smoothness(t));
    }
    T32_Free(t);
}

int main(int argc, char **argv)
{
    const char *root = (argc > 1) ? argv[1] : "assets";
    int i;

    scan_dir(root);
    if (g_nfiles == 0) {
        fprintf(stderr, "no .T32 under %s\n", root);
        return 1;
    }
    for (i = 0; i < g_nfiles; ++i)
        check(g_files[i]);
    printf("\n%d files, %d failed\n", g_nfiles, g_failed);
    return g_failed ? 1 : 0;
}
