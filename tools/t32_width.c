/*
 * t32_width - определяет ШИРИНУ растра T32 математически, без угадывания.
 *
 * Канонический deswizzle PSMCT8 (unswizzle8_ps2 из ps2-tim2-tool) - это
 * отображение пиксель(x,y) -> смещение байта si(x,y). Оно является ТОЧНОЙ
 * перестановкой диапазона 0..width*height-1 только при ВЕРНОЙ ширине-питче.
 * При неверной ширине часть si выходит за n (пропуски) и/или несколько
 * пикселей дают один si (дубликаты).
 *
 * Поэтому перебираем все ширины-кандидаты (делители raster_size, кратные 16,
 * с высотой кратной 16) и печатаем для каждой число пропусков и дублей.
 * Верная ширина даёт пропуски=0, дубли=0.
 *
 * Usage:
 *   t32_width FILE.T32
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CLUT 0x400

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Возвращает 1 если канонический deswizzle при (width,height) - биекция. */
static int test_perm(uint8_t *used, int width, int height,
                     long *miss, long *dup)
{
    size_t n = (size_t)width * height;
    int x, y;
    *miss = 0; *dup = 0;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            int block_location  = (y & (~0xf)) * width + (x & (~0xf)) * 2;
            int swap_selector   = (((y + 2) >> 2) & 1) * 4;
            int posY            = (((y & (~3)) >> 1) + (y & 1)) & 7;
            int column_location = posY * width * 2 + ((x + swap_selector) & 7) * 4;
            int byte_num        = ((y >> 1) & 1) + ((x >> 2) & 2);
            size_t si = (size_t)block_location + column_location + byte_num;
            if (si >= n) ++*miss;
            else if (++used[si] > 1) ++*dup;
        }
    return (*miss == 0 && *dup == 0);
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : NULL;
    FILE *fp;
    long size;
    uint8_t *buf;
    uint32_t raster, payload, raster_off;
    uint8_t *used;
    uint32_t w;

    if (!path) { fprintf(stderr, "usage: t32_width FILE.T32\n"); return 1; }
    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "open fail\n"); return 1; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) return 1;
    fclose(fp);

    raster  = (rd32(buf + 0x04) >> 16) << 8;
    payload = rd32(buf + 0x0C);
    (void)raster_off;
    used = (uint8_t *)malloc(raster);

    printf("raster=0x%X (%u bytes). width must give a bijection.\n",
           raster, raster);
    printf("%-8s %-8s %-8s %-8s %s\n", "width", "height", "miss", "dup", "bijection?");
    for (w = 16; w <= raster / 16; w += 16) {
        uint32_t h;
        long miss, dup;
        int ok;
        if (raster % w) continue;
        h = raster / w;
        if (h % 16) continue;
        memset(used, 0, raster);
        ok = test_perm(used, (int)w, (int)h, &miss, &dup);
        if (ok || miss < (long)raster / 4) /* печатаем близкие к биекции */
            printf("%-8u %-8u %-8ld %-8ld %s\n", w, h, miss, dup,
                   ok ? "<<< YES" : "");
    }
    free(used);
    return 0;
}
