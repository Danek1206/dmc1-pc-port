/*
 * DMC1 PC Port - T32 (архив текстур интерфейса) loader interface
 *
 * Как и TIM2, модуль не зависит от OpenGL: разбирает файл и отдаёт RGBA8.
 */

#ifndef DMC1_LOADER_T32_H
#define DMC1_LOADER_T32_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t record_count;   /* 0x00 */
    uint32_t raster_size;    /* 0x04: старшие 16 бит, умноженные на 0x100 */
    uint32_t table_end;      /* 0x08 = 0x20 + record_count*0x18 */
    uint32_t payload_size;   /* 0x0C: растр плюс хвостовые пакеты */
    uint32_t width;          /* SMPS из GS-регистра TRXPOS */
    uint32_t height;         /* raster_size / width, на диске всегда 256 */
    uint32_t clut_entries;   /* всегда 256 */
    uint32_t raster_offset;
    uint32_t clut_offset;

    const uint8_t *pixels;   /* ширина*высота байтов-индексов */
    const uint8_t *clut;     /* 256 записей по 4 байта */

    uint8_t *file_buffer;    /* владеет им, когда owned != 0 */
    int owned;
} T32Texture;

/* Признак T32 по содержимому: заголовок согласуется с размером файла. */
int T32_Matches(const uint8_t *data, size_t size);

T32Texture *T32_Parse(const uint8_t *data, size_t size, const char *origin);
T32Texture *T32_LoadFile(const char *path);

int T32_Decode(const T32Texture *t, uint8_t *rgba);
int T32_DecodeAlloc(const T32Texture *t, uint8_t **out_rgba);

void T32_Free(T32Texture *t);

#endif /* DMC1_LOADER_T32_H */
