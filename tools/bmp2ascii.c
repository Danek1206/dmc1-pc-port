/*
 * bmp2ascii - печатает 8bpp BMP как ASCII-карту яркости.
 *
 * Текстовому агенту нужен «глаз» без GUI: читаем наш же BMP (256-цветная
 * палитра, строки снизу вверх), считаем яркость палитры и рисуем даунсемпл
 * символами ' .:-=+*#%@'. Так видно, собран ли спрайт или это шум.
 *
 * Usage:
 *   bmp2ascii FILE.bmp [WIDTH]   (default WIDTH=120 колонок)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : NULL;
    int cols = (argc > 2) ? atoi(argv[2]) : 120;
    FILE *fp;
    uint8_t *buf;
    long size;
    int w, h, bpp, row;
    uint8_t *pixels, *pal;
    double lum[256];
    const char *ramp = " .:-=+*#%@";
    int cy, cx;
    int rows_out;

    if (!path) { fprintf(stderr, "usage: bmp2ascii FILE.bmp [COLS]\n"); return 1; }
    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "open fail\n"); return 1; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) return 1;
    fclose(fp);

    if (buf[0] != 'B' || buf[1] != 'M') { fprintf(stderr, "not BMP\n"); return 1; }
    w = (int)rd32(buf + 18);
    h = (int)rd32(buf + 22);
    bpp = rd16(buf + 28);
    if (bpp != 8) { fprintf(stderr, "only 8bpp supported (got %d)\n", bpp); return 1; }
    row = (w + 3) & ~3;
    pal = buf + 54;
    pixels = buf + 54 + 1024;

    for (int i = 0; i < 256; ++i)
        lum[i] = (pal[i*4]*30.0 + pal[i*4+1]*59.0 + pal[i*4+2]*11.0) / 100.0;

    if (cols > w) cols = w;
    rows_out = (int)((double)cols * h / w / 2.0);
    if (rows_out < 1) rows_out = 1;

    printf("%s  %dx%d  ->  %dx%d ascii\n", path, w, h, cols, rows_out);
    for (cy = 0; cy < rows_out; ++cy) {
        int sy0 = cy * h / rows_out, sy1 = (cy + 1) * h / rows_out;
        for (cx = 0; cx < cols; ++cx) {
            int sx0 = cx * w / cols, sx1 = (cx + 1) * w / cols;
            double sum = 0.0; int cnt = 0; int y, x;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (y = sy0; y < sy1; ++y) {
                const uint8_t *r = pixels + (size_t)(h - 1 - y) * row;
                for (x = sx0; x < sx1; ++x) { sum += lum[r[x]]; ++cnt; }
            }
            {
                double avg = cnt ? sum / cnt : 0.0;
                int idx = (int)(avg / 255.0 * 9.0);
                if (idx < 0) idx = 0; if (idx > 9) idx = 9;
                putchar(ramp[idx]);
            }
        }
        putchar('\n');
    }
    return 0;
}
