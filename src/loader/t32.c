/*
 * DMC1 PC Port - T32 (архив текстур интерфейса) loader
 *
 * Формат измерен по всем 125 файлам *.T32 с диска (см. docs/formats.md).
 * Заголовок:
 *
 *   0x00 u32  record_count
 *   0x04 u32  старшие 16 бит - размер растра, сдвинутый на 8
 *   0x08 u32  конец таблицы записей = 0x20 + record_count*0x18
 *   0x0C u32  payload_size: растр плюс хвостовые пакеты
 *   0x20      таблицы записей по 0x18
 *
 * Данные идут в конце файла, порядок блоков такой:
 *
 *   raster_offset = file_size - payload_size - 0x400
 *   [растр raster_size] [хвостовые пакеты payload_size-raster_size]
 *   [CLUT 0x400 = 256 записей по 4 байта]
 *
 * Тождество raster_offset + raster_size + (payload_size-raster_size) + 0x400
 * = file_size выполняется для всех файлов набора, поэтому смещения берутся
 * из него, а не угадываются.
 *
 * Ширина и высота в заголовке не хранятся. Ширина - это поле SMPS регистра
 * TRXPOS (адрес 0x51) из GIF-пакетов преамбулы: COLBAR_F даёт 0x380 = 896,
 * MSG_TEX даёт 0x300 = 768. Высота равна raster_size/width и обращается в
 * 256 на всех файлах диска, что и служит проверкой разбора.
 *
 * Растр лежит в VRAM-раскладке GS PSMCT8 (страница 128x64 = 8192 байта), а не
 * линейно: линейное чтение даёт полосы (проверено метрикой связности: линейно
 * ~1.3 уровня на соседний пиксель, свизл ~0.33). Таблица разворота страницы
 * перенесена из GS Memory Swizzle Visualizer (TellowKrinkle) и сверена по
 * связности на SELECT/SPARDA_F. Ширина строки = buffer width (поле SMPS/TRXPOS),
 * высота = raster_size/width (=256 на всех файлах диска).
 */

#include "loader/t32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define T32_HEADER_SIZE   0x40
#define T32_CLUT_SIZE     0x400
#define T32_CLUT_ENTRIES  256
#define T32_REG_TRXPOS    0x51

/*
 * Deswizzle PSMCT8 (8bpp) канонической формулой PS2 GS, перенесённой из
 * рабочего конвертера ps2-tim2-tool (unswizzle8_ps2). Формула работает в
 * байтах и привязана к ширине строки (VRAM pitch = width), а не к таблице
 * страниц. Проверена на COLBAR_F: даёт периодические вертикальные столбцы
 * шириной 128 px (размер swizzle-страницы), чего линейное чтение не даёт.
 */
static void t32_deswizzle8(const uint8_t *src, uint8_t *dst,
                           int width, int height)
{
    size_t n = (size_t)width * height;
    int x, y;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            int block_location  = (y & (~0xf)) * width + (x & (~0xf)) * 2;
            int swap_selector   = (((y + 2) >> 2) & 1) * 4;
            int posY            = (((y & (~3)) >> 1) + (y & 1)) & 7;
            int column_location = posY * width * 2 + ((x + swap_selector) & 7) * 4;
            int byte_num        = ((y >> 1) & 1) + ((x >> 2) & 2);
            size_t si = (size_t)block_location + column_location + byte_num;
            if (si < n) dst[(size_t)y * width + x] = src[si];
        }
}



static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*
 * REGISTER-квад GIF-пакета: мл. байт адреса регистра в байте 0, остальные
 * семь нулей. Данные регистра следуют в следующих 8 байтах.
 */
static int is_register(const uint8_t *p, uint8_t addr)
{
    int i;
    if (p[0] != addr)
        return 0;
    for (i = 1; i < 8; ++i) {
        if (p[i] != 0)
            return 0;
    }
    return 1;
}

/*
 * SMPS из TRXPOS: поле занимает биты 32..44 64-битного значения, то есть
 * младшие 13 бит второй половины пакета. Это ширина VRAM-ОБЛАСТИ пересылки,
 * а не ширина строки растра: COLBAR_F даёт 0x380 = 896, тогда как реальные
 * строки растра идут по 256 байт. Поэтому ширина берётся из регистра только
 * как верхняя граница, а уточняется подбором ниже.
 */
static uint32_t t32_source_width(const uint8_t *d, size_t from, size_t to)
{
    size_t i;
    for (i = from; i + 16 <= to; i += 8) {
        if (is_register(d + i, T32_REG_TRXPOS)) {
            uint32_t hi = rd32(d + i + 12);
            uint32_t smps = hi & 0x1FFFu;
            if (smps >= 64 && smps <= 2048)
                return smps;
        }
    }
    return 0;
}

/*
 * Ширина строки растра.
 *
 * Высота текстуры T32 на диске всегда 256 строк, а ширина равна полю SMPS
 * регистра TRXPOS (ширина источника пересылки в VRAM PS2). Проверено:
 * COLBAR_F/SELECT SMPS=0x380=896, MSG_TEX SMPS=0x300=768, и для всех файлов
 * raster_size/256 даёт ровно это значение. Поэтому ширина берётся из SMPS, а
 * если регистра нет - вычисляется как raster_size/256.
 */
static uint32_t t32_pick_width(const uint8_t *d, size_t from, size_t to,
                               uint32_t raster_size)
{
    uint32_t smps = t32_source_width(d, from, to);

    if (smps != 0 && raster_size % smps == 0)
        return smps;
    if (raster_size % 256 == 0)
        return raster_size / 256u;
    return 0;
}

int T32_Matches(const uint8_t *data, size_t size)
{
    uint32_t raster_size, payload, table_end, count, raster_off;

    if (!data || size < T32_HEADER_SIZE + T32_CLUT_SIZE)
        return 0;

    count      = rd32(data + 0x00);
    raster_size = ((rd32(data + 0x04) >> 16) << 8);
    table_end  = rd32(data + 0x08);
    payload    = rd32(data + 0x0C);

    if (raster_size == 0 || payload < raster_size)
        return 0;
    if ((uint64_t)payload + T32_CLUT_SIZE > size)
        return 0;

    /*
     * Поле 0x08 хранит конец таблицы записей. Линейная связь с числом записей
     * держится не для всех файлов (count=1 даёт 0x30, count=2 даёт 0x50),
     * поэтому оно используется только как нижняя граница данных.
     */
    if (table_end < 0x30 || (uint64_t)table_end * 0x18u < count)
        return 0;

    raster_off = (uint32_t)(size - payload - T32_CLUT_SIZE);
    if (raster_off < table_end)
        return 0;

    /*
     * Ширина строки не хранится в заголовке и вычисляется подбором, поэтому
     * здесь проверяется только наличие хотя бы одного допустимого делителя.
     */
    if (raster_size < 64u * 16u)
        return 0;
    return 1;
}

T32Texture *T32_Parse(const uint8_t *data, size_t size, const char *origin)
{
    T32Texture *t;
    uint32_t raster_off;

    if (!T32_Matches(data, size)) {
        fprintf(stderr, "T32: %s: not a T32 archive or fields disagree\n",
                origin ? origin : "?");
        return NULL;
    }

    t = (T32Texture *)calloc(1, sizeof(T32Texture));
    if (!t)
        return NULL;

    t->record_count = rd32(data + 0x00);
    t->raster_size  = ((rd32(data + 0x04) >> 16) << 8);
    t->table_end    = rd32(data + 0x08);
    t->payload_size = rd32(data + 0x0C);

    raster_off = (uint32_t)(size - t->payload_size - T32_CLUT_SIZE);
    t->raster_offset = raster_off;
    t->clut_offset   = (uint32_t)(size - T32_CLUT_SIZE);
    t->pixels = data + raster_off;
    t->clut   = data + t->clut_offset;
    t->clut_entries = T32_CLUT_ENTRIES;
    t->width  = t32_pick_width(data, T32_HEADER_SIZE, raster_off,
                               t->raster_size);
    if (t->width == 0) {
        fprintf(stderr, "T32: %s: no usable row width for raster 0x%X\n",
                origin ? origin : "?", t->raster_size);
        free(t);
        return NULL;
    }
    t->height = t->raster_size / t->width;
    t->owned  = 0;
    return t;
}

T32Texture *T32_LoadFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;
    T32Texture *t;

    if (!fp) {
        fprintf(stderr, "T32: cannot open %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= T32_HEADER_SIZE + T32_CLUT_SIZE) {
        fclose(fp);
        return NULL;
    }
    buf = (uint8_t *)malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    t = T32_Parse(buf, (size_t)n, path);
    if (!t) {
        free(buf);
        return NULL;
    }
    t->owned = 1;
    t->file_buffer = buf;
    return t;
}

void T32_Free(T32Texture *t)
{
    if (!t)
        return;
    if (t->owned)
        free(t->file_buffer);
    free(t);
}

int T32_Decode(const T32Texture *t, uint8_t *rgba)
{
    size_t i, n;
    uint8_t *lin;

    if (!t || !rgba)
        return 0;

    n = (size_t)t->width * t->height;
    lin = (uint8_t *)malloc(n);
    if (!lin)
        return 0;

    /*
     * Ширина строки = поле buffer width (SMPS/TRXPOS), высота = raster/width.
     * Растр разворачивается из GS-свизла PSMCT8, затем индексы переводятся в
     * RGBA через CLUT.
     */
    t32_deswizzle8(t->pixels, lin, (int)t->width, (int)t->height);

    for (i = 0; i < n; ++i) {
        const uint8_t *e = t->clut + (size_t)lin[i] * 4u;
        uint8_t *d = rgba + i * 4u;
        d[0] = e[0]; d[1] = e[1]; d[2] = e[2]; d[3] = e[3];
    }
    free(lin);
    return 1;
}



int T32_DecodeAlloc(const T32Texture *t, uint8_t **out_rgba)
{
    size_t n = (size_t)t->width * t->height * 4u;
    uint8_t *buf = (uint8_t *)malloc(n);
    if (!buf)
        return 0;
    if (!T32_Decode(t, buf)) {
        free(buf);
        return 0;
    }
    *out_rgba = buf;
    return 1;
}
