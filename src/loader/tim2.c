/*
 * DMC1 PC Port - TIM2 (PS2 texture) loader
 *
 * Формат измерен по файлам E:\DATA\ETC\*.TM2 (см. docs/formats.md):
 *
 *   0x00 u32  id 'TIM2'
 *   0x04 u16  version (4)
 *   0x06 u16  flags - НЕ является форматом пикселей: у 8bpp и 32bpp файлов
 *                   одинаковое значение 0x0001
 *   0x08 u32  tex_ofs (0 = данные идут сразу за заголовком)
 *   0x0C u32  clut_ofs (0)
 *   0x10 u32  размер всего, что следует после 16-байтного тега
 *   0x14 u32  размер данных CLUT (0 = палитры нет)
 *   0x18 u32  размер данных растра
 *   0x1C u32  помечен как неизвестный, см. docs/formats.md
 *   0x20 u8   0
 *   0x21 u8   1
 *   0x22 u8   формат пикселей: 0x03 = 8bpp, 0x00 = 32bpp, 0x43 = 4bpp
 *   0x23 u8   сопутствующий байт формата (5 / 3 / 4)
 *   0x24 u16  ширина растра
 *   0x26 u16  высота растра
 *   0x28..0x3F прочие поля, загрузка их не требует
 *   0x40      данные растра
 *   0x40+img  данные CLUT
 *
 * Глубина цвета однозначно выводится из img_size/(width*height), что и
 * используется как основной источник истины: поле 0x22 служит только
 * перекрёстной проверкой.
 */

#include "loader/tim2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIM2_HEADER_SIZE 0x40
#define TIM2_ID          0x324D4954u   /* 'TIM2' little-endian */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*
 * PS2 хранит 16-битные пиксели в формате PSMT16 = 5-5-5-1 (A = 1 бит).
 * Значение 0x8000 означает полностью прозрачный пиксель, как в оригинале.
 */
static void unpack16(uint16_t v, uint8_t out[4])
{
    uint16_t r = v & 0x1F, g = (v >> 5) & 0x1F, b = (v >> 10) & 0x1F;
    out[0] = (uint8_t)((r * 255 + 15) / 31);
    out[1] = (uint8_t)((g * 255 + 15) / 31);
    out[2] = (uint8_t)((b * 255 + 15) / 31);
    out[3] = (v & 0x8000) ? 0xFF : 0x00;
}

TIM2Texture *TIM2_Parse(const uint8_t *data, size_t size, const char *origin)
{
    const uint8_t *h;
    TIM2Texture *t;
    uint64_t pixels;
    uint32_t img_size, clut_size;
    unsigned bpp;

    if (!data || size < TIM2_HEADER_SIZE)
        return NULL;
    h = data;

    if (rd32(h) != TIM2_ID) {
        fprintf(stderr, "TIM2: %s: id is not 'TIM2'\n", origin ? origin : "?");
        return NULL;
    }

    t = (TIM2Texture *)calloc(1, sizeof(TIM2Texture));
    if (!t)
        return NULL;

    t->version   = rd16(h + 0x04);
    t->flags     = rd16(h + 0x06);
    t->total_after_tag = rd32(h + 0x10);
    t->clut_size = rd32(h + 0x14);
    t->img_size  = rd32(h + 0x18);
    t->format_byte = h[0x22];
    t->format_aux  = h[0x23];
    t->width     = rd16(h + 0x24);
    t->height    = rd16(h + 0x26);

    img_size  = t->img_size;
    clut_size = t->clut_size;

    if (t->width == 0 || t->height == 0) {
        fprintf(stderr, "TIM2: %s: bad dimensions %ux%u\n",
                origin ? origin : "?", t->width, t->height);
        free(t);
        return NULL;
    }

    /* Глубина цвета выводится из фактического объёма данных растра. */
    pixels = (uint64_t)t->width * t->height;
    if (pixels == 0 || (uint64_t)img_size * 8u % pixels != 0) {
        fprintf(stderr, "TIM2: %s: img_size %u does not fit %ux%u\n",
                origin ? origin : "?", img_size, t->width, t->height);
        free(t);
        return NULL;
    }
    bpp = (unsigned)((uint64_t)img_size * 8u / pixels);
    if (bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) {
        fprintf(stderr, "TIM2: %s: unsupported bpp %u\n", origin ? origin : "?", bpp);
        free(t);
        return NULL;
    }
    t->bpp = bpp;

    /* Расположение данных проверено на всём наборе .TM2 с диска. */
    if ((uint64_t)TIM2_HEADER_SIZE + img_size + clut_size > size) {
        fprintf(stderr, "TIM2: %s: truncated (need 0x%X, have 0x%zX)\n",
                origin ? origin : "?",
                (unsigned)(TIM2_HEADER_SIZE + img_size + clut_size), size);
        free(t);
        return NULL;
    }
    t->pixels = data + TIM2_HEADER_SIZE;
    t->clut   = clut_size ? data + TIM2_HEADER_SIZE + img_size : NULL;
    t->clut_entries = clut_size / 4u;

    if (clut_size && clut_size % 4) {
        fprintf(stderr, "TIM2: %s: CLUT size %u not a multiple of 4\n",
                origin ? origin : "?", clut_size);
        free(t);
        return NULL;
    }

    t->owned = 0;      /* буфер принадлежит вызывающему */
    return t;
}

TIM2Texture *TIM2_LoadFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;
    TIM2Texture *t;

    if (!fp) {
        fprintf(stderr, "TIM2: cannot open %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= TIM2_HEADER_SIZE) {
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

    t = TIM2_Parse(buf, (size_t)n, path);
    if (!t) {
        free(buf);
        return NULL;
    }
    t->owned = 1;
    t->file_buffer = buf;
    return t;
}

void TIM2_Free(TIM2Texture *t)
{
    if (!t)
        return;
    /*
     * pixels и clut указывают внутрь file_buffer (один malloc в TIM2_LoadFile),
     * поэтому освобождается только он.
     */
    if (t->owned)
        free(t->file_buffer);
    free(t);
}

/*
 * CLUT-запись в DMC лежит как 4 байта. Порядок R и B подтверждается визуально
 * по выгрузке (см. tools/dmc_view): в CAPCOM.TM2 первые записи - серая шкала,
 * поэтому различить их можно только на цветной текстуре.
 */
static void clut_color(const TIM2Texture *t, uint32_t index, uint8_t out[4])
{
    const uint8_t *e;
    if (!t->clut) {
        out[0] = out[1] = out[2] = 0;
        out[3] = 0xFF;
        return;
    }
    if (index >= t->clut_entries) {
        /* Индекс вне сохранённой палитры: прозрачный, а не мусорный пиксель. */
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    e = t->clut + index * 4u;
    out[0] = e[0];
    out[1] = e[1];
    out[2] = e[2];
    out[3] = e[3];
}

int TIM2_Decode(const TIM2Texture *t, uint8_t *rgba)
{
    uint32_t x, y;
    const uint32_t w = t->width, h = t->height;

    if (!t || !rgba)
        return 0;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            uint8_t *dst = rgba + ((size_t)y * w + x) * 4u;
            switch (t->bpp) {
            case 32: {
                const uint8_t *p = t->pixels + ((size_t)y * w + x) * 4u;
                dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2]; dst[3] = p[3];
                break;
            }
            case 24: {
                const uint8_t *p = t->pixels + ((size_t)y * w + x) * 3u;
                dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2]; dst[3] = 0xFF;
                break;
            }
            case 16: {
                const uint8_t *p = t->pixels + ((size_t)y * w + x) * 2u;
                unpack16((uint16_t)(p[0] | (p[1] << 8)), dst);
                break;
            }
            case 8: {
                uint8_t idx = t->pixels[(size_t)y * w + x];
                clut_color(t, idx, dst);
                break;
            }
            case 4:
            default: {
                /*
                 * PSMT4: два пикселя в байте. Какой из полубайтов левый,
                 * проверяется выгрузкой; здесь принят младший полубайт как
                 * левый, что соответствует упаковке PS2.
                 */
                uint8_t byte = t->pixels[((size_t)y * w + x) >> 1];
                uint8_t idx = (x & 1u) ? (byte & 0x0F) : (byte >> 4);
                clut_color(t, idx, dst);
                break;
            }
            }
        }
    }
    return 1;
}

int TIM2_DecodeAlloc(const TIM2Texture *t, uint8_t **out_rgba)
{
    size_t n = (size_t)t->width * t->height * 4u;
    uint8_t *buf = (uint8_t *)malloc(n);
    if (!buf)
        return 0;
    if (!TIM2_Decode(t, buf)) {
        free(buf);
        return 0;
    }
    *out_rgba = buf;
    return 1;
}
