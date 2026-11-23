/*
 * DMC1 PC Port - TIM2 (PS2 texture) loader interface
 *
 * Без зависимостей от OpenGL: модуль только разбирает файл и выдаёт RGBA8,
 * поэтому его используют и движок, и автономные утилиты вроде tools/dmc_view.
 */

#ifndef DMC1_LOADER_TIM2_H
#define DMC1_LOADER_TIM2_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t version;
    uint32_t flags;           /* 0x06: не является форматом пикселей */
    uint32_t total_after_tag; /* 0x10 */
    uint32_t clut_size;       /* 0x14 */
    uint32_t img_size;        /* 0x18 */
    uint8_t  format_byte;     /* 0x22: 0x03=8bpp, 0x00=32bpp, 0x43=4bpp */
    uint8_t  format_aux;      /* 0x23 */
    uint32_t width;           /* 0x24 */
    uint32_t height;          /* 0x26 */
    uint32_t bpp;             /* выведено из img_size/(w*h) */
    uint32_t clut_entries;    /* clut_size/4 */

    const uint8_t *pixels;    /* внутрь file_buffer при загрузке из файла */
    const uint8_t *clut;      /* NULL, если палитры нет */

    uint8_t *file_buffer;     /* владеет им, когда owned != 0 */
    int owned;
} TIM2Texture;

/* Разбор из уже прочитанного буфера. data должна пережить текстуру. */
TIM2Texture *TIM2_Parse(const uint8_t *data, size_t size, const char *origin);

/* Чтение файла целиком; владение данными у текстуры. */
TIM2Texture *TIM2_LoadFile(const char *path);

/* Заполняет width*height*4 байт RGBA8. Возвращает 1 при успехе. */
int TIM2_Decode(const TIM2Texture *t, uint8_t *rgba);

/* То же, но буфер выделяется сам; освобождается вызывающим через free(). */
int TIM2_DecodeAlloc(const TIM2Texture *t, uint8_t **out_rgba);

void TIM2_Free(TIM2Texture *t);

#endif /* DMC1_LOADER_TIM2_H */
