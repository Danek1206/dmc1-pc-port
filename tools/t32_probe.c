/*
 * t32_probe - эмпирический подбор раскладки растра T32.
 *
 * Размеры текстуры в GS-регистрах T32 не прописаны (BITBLT/TRXREG нулевые),
 * а растр лежит в VRAM-раскладке PSMCT8 (страница 128x64 = 8192 байта).
 * Поэтому перебираем ВСЕ допустимые пары (width,height), для каждой
 * раскладываем свизл и считаем метрику связности:
 *
 *   coherence = среднее |L(x+1,y)-L(x,y)| + |L(x,y+1)-L(x,y)| по яркости
 *               палитры. У правильной раскладки соседние пиксели похожи,
 *               значит значение маленькое; у шума - десятки уровней.
 *
 * Для каждого кандидата пишется BMP в OUT_DIR, а в stdout печатается таблица,
 * отсортированная по возрастанию coherence. Так выбор перестаёт быть угадыванием.
 *
 * Usage:
 *   t32_probe FILE.T32 [OUT_DIR]   (default build\probe)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <windows.h>

#define T32_CLUT_SIZE 0x400

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*
 * Точная свизл-таблица PSMCT8, перенесённая из GS Memory Swizzle Visualizer
 * (TellowKrinkle). Страница = 128x64 пикселя = 8192 байта. pageSwizzle берётся
 * из PSMCT32 (для 8bpp раскладка страниц та же), blockSwizzle - из PSMT8.
 *
 * table[y*128 + x] = смещение байта внутри страницы, где лежит пиксель (x,y)
 * линейного изображения. То есть src[page*8192 + table[y*128+x]].
 */
static const int PAGE_SWIZZLE[32] = {
     0,  1,  4,  5, 16, 17, 20, 21,
     2,  3,  6,  7, 18, 19, 22, 23,
     8,  9, 12, 13, 24, 25, 28, 29,
    10, 11, 14, 15, 26, 27, 30, 31,
};
static const int COL_SWIZZLE8[128] = {
     0,  4, 16, 20, 32, 36, 48, 52,  2,  6, 18, 22, 34, 38, 50, 54,
     8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62,
    33, 37, 49, 53,  1,  5, 17, 21, 35, 39, 51, 55,  3,  7, 19, 23,
    41, 45, 57, 61,  9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31,
    32, 36, 48, 52,  0,  4, 16, 20, 34, 38, 50, 54,  2,  6, 18, 22,
    40, 44, 56, 60,  8, 12, 24, 28, 42, 46, 58, 62, 10, 14, 26, 30,
     1,  5, 17, 21, 33, 37, 49, 53,  3,  7, 19, 23, 35, 39, 51, 55,
     9, 13, 25, 29, 41, 45, 57, 61, 11, 15, 27, 31, 43, 47, 59, 63,
};

static int g_table[128 * 64];

static void build_swizzle_table(void)
{
    int block, i;
    for (block = 0; block < 32; ++block) {
        int base = PAGE_SWIZZLE[block] * 256;
        int bxpx = (block % 8) * 16;
        int bypx = (block / 8) * 16;
        for (i = 0; i < 256; ++i) {
            int colOff = COL_SWIZZLE8[i % 128];
            int pos = base + colOff + (i / 64) * 64;
            int x = bxpx + (i % 16);
            int y = bypx + (i / 16);
            if (x < 128 && y < 64)
                g_table[y * 128 + x] = pos;
        }
    }
}

/*
 * Канонический deswizzle PSMCT8, перенесённый из рабочего конвертера
 * ps2-tim2-tool (unswizzle8_ps2). Работает в байтах напрямую, привязан к
 * ширине строки (VRAM pitch = width). Это формула PS2 GS, сверенная с железом,
 * а не догадка из визуализатора.
 */
static void deswizzle8(const uint8_t *src, uint8_t *dst,
                       int width, int height, int pitch)
{
    size_t n = (size_t)width * height;
    int x, y;
    (void)pitch;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            int block_location = (y & (~0xf)) * width + (x & (~0xf)) * 2;
            int swap_selector  = (((y + 2) >> 2) & 1) * 4;
            int posY            = (((y & (~3)) >> 1) + (y & 1)) & 7;
            int column_location = posY * width * 2 + ((x + swap_selector) & 7) * 4;
            int byte_num        = ((y >> 1) & 1) + ((x >> 2) & 2);
            size_t si = (size_t)block_location + column_location + byte_num;
            if (si < n) dst[(size_t)y * width + x] = src[si];
        }
}

/* Линейное прочтение (для сравнения). */
static void linear8(const uint8_t *src, uint8_t *dst, int width, int height)
{
    memcpy(dst, src, (size_t)width * height);
}

static double coherence(const uint8_t *lin, int width, int height,
                        const double *lum)
{
    double sum = 0.0;
    long n = 0;
    int x, y;
    for (y = 0; y < height; ++y)
        for (x = 0; x + 1 < width; ++x) {
            sum += fabs(lum[lin[y * width + x + 1]] - lum[lin[y * width + x]]);
            ++n;
        }
    for (y = 0; y + 1 < height; ++y)
        for (x = 0; x < width; ++x) {
            sum += fabs(lum[lin[(y + 1) * width + x]] - lum[lin[y * width + x]]);
            ++n;
        }
    return n ? sum / (double)n : 1e9;
}

/* Запись линейного 8bpp+CLUT в BMP. */
static void write_bmp(const char *path, const uint8_t *lin, int width,
                      int height, const uint8_t *clut)
{
    FILE *fp = fopen(path, "wb");
    int row = (width + 3) & ~3;
    int pal[256];
    uint8_t pad[4] = {0, 0, 0, 0};
    uint8_t *rows;
    int x, y, i;
    int bfr;
    int file_size;
    uint8_t hdr[54];

    if (!fp)
        return;
    for (i = 0; i < 256; ++i)
        pal[i] = clut[i * 4] | (clut[i * 4 + 1] << 8) | (clut[i * 4 + 2] << 16);

    rows = (uint8_t *)malloc((size_t)row * height);
    for (y = 0; y < height; ++y) {
        uint8_t *d = rows + (size_t)(height - 1 - y) * row;
        const uint8_t *s = lin + (size_t)y * width;
        for (x = 0; x < width; ++x)
            d[x] = s[x];
        for (x = width; x < row; ++x)
            d[x] = 0;
    }

    bfr = row * height;
    file_size = 54 + 1024 + bfr;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = file_size; hdr[3] = file_size >> 8;
    hdr[4] = file_size >> 16; hdr[5] = file_size >> 24;
    hdr[10] = 54 + 1024;
    hdr[14] = 40;
    hdr[18] = width; hdr[19] = width >> 8; hdr[20] = width >> 16; hdr[21] = width >> 24;
    hdr[22] = height; hdr[23] = height >> 8; hdr[24] = height >> 16; hdr[25] = height >> 24;
    hdr[26] = 1;
    hdr[28] = 8;
    hdr[34] = bfr;
    fwrite(hdr, 1, 54, fp);
    fwrite(pal, 4, 256, fp);
    fwrite(rows, 1, bfr, fp);
    (void)pad;
    fclose(fp);
    free(rows);
}

typedef struct { int w, h, swz; double coh; char name[64]; } Cand;

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : NULL;
    const char *outdir = (argc > 2) ? argv[2] : "build\\probe";
    FILE *fp;
    long size;
    uint8_t *buf;
    uint32_t raster, payload, raster_off;
    uint8_t *clut;
    double lum[256];
    uint8_t *lin;
    Cand cands[512];
    int nc = 0;
    int i, j;
    uint32_t npages;

    if (!path) {
        fprintf(stderr, "usage: t32_probe FILE.T32 [OUT_DIR]\n");
        return 1;
    }
    build_swizzle_table();
    CreateDirectoryA(outdir, NULL);

    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "open fail\n"); return 1; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) { return 1; }
    fclose(fp);

    raster  = (rd32(buf + 0x04) >> 16) << 8;
    payload = rd32(buf + 0x0C);
    raster_off = (uint32_t)(size - payload - T32_CLUT_SIZE);
    clut = buf + (size - T32_CLUT_SIZE);
    lin = (uint8_t *)malloc(raster ? raster : 1);

    for (i = 0; i < 256; ++i)
        lum[i] = (clut[i*4]*30.0 + clut[i*4+1]*59.0 + clut[i*4+2]*11.0) / 100.0;

    npages = raster / 8192;
    if (raster % 8192 != 0) {
        fprintf(stderr, "raster 0x%X not a whole number of PSMCT8 pages\n", raster);
    }

    /* Все пары (pgx,pgy) страниц: pgx*pgy = npages. swizzle width=pgx*128. */
    for (j = 1; (uint32_t)j <= npages; ++j) {
        if (npages % (uint32_t)j) continue;
        int pgx = j, pgy = (int)(npages / (uint32_t)j);
        int w = pgx * 128, h = pgy * 64;
        Cand *c;
        if (w > (int)raster) continue;
        c = &cands[nc++];
        c->w = w; c->h = h; c->swz = 1;
        deswizzle8(buf + raster_off, lin, w, h, w);
        c->coh = coherence(lin, w, h, lum);
        _snprintf(c->name, sizeof(c->name) - 1, "%s_swx%dx%d", "", w, h);
        {
            char outp[512];
            _snprintf(outp, sizeof(outp) - 1, "%s\\%u_sw_%dx%d.bmp", outdir,
                      (unsigned)(raster), w, h);
            write_bmp(outp, lin, w, h, clut);
        }
    }

    /* Линейные варианты для известных высот 256 и 64. */
    {
        int known_w[8] = {128, 256, 384, 448, 512, 768, 896, 1024};
        for (i = 0; i < 8; ++i) {
            int w = known_w[i];
            int h;
            Cand *c;
            if (w == 0 || (uint32_t)w > raster) continue;
            if (raster % w) continue;
            h = (int)(raster / w);
            c = &cands[nc++];
            c->w = w; c->h = h; c->swz = 0;
            linear8(buf + raster_off, lin, w, h);
            c->coh = coherence(lin, w, h, lum);
            _snprintf(c->name, sizeof(c->name) - 1, "lin_%dx%d", w, h);
            {
                char outp[512];
                _snprintf(outp, sizeof(outp) - 1, "%s\\%u_lin_%dx%d.bmp", outdir,
                          (unsigned)(raster), w, h);
                write_bmp(outp, lin, w, h, clut);
            }
        }
    }

    /* сортировка по возрастанию coherence */
    for (i = 1; i < nc; ++i) {
        Cand t = cands[i];
        for (j = i; j > 0 && cands[j - 1].coh > t.coh; --j)
            cands[j] = cands[j - 1];
        cands[j] = t;
    }

    printf("%-16s  %5s %5s  %-4s  %s\n", "candidate", "w", "h", "mode", "coherence");
    for (i = 0; i < nc; ++i)
        printf("%-16s  %5d %5d  %-4s  %.3f\n", "", cands[i].w, cands[i].h,
               cands[i].swz ? "swz8" : "lin8", cands[i].coh);
    printf("\nbest: %dx%d %s coherence=%.3f\n", cands[0].w, cands[0].h,
           cands[0].swz ? "swizzle PSMCT8" : "linear", cands[0].coh);
    return 0;
}
