/*
 * DMC1 PC Port - TIM2 Texture Loader
 * Загрузка PS2 TIM2 текстур в OpenGL
 * 
 * TIM2 формат:
 * - Header: 20 bytes
 * - Magic: "TIM2" (0x324D4954)
 * - Compression: 4bpp, 8bpp, 16bpp, 24bpp, 32bpp
 * - Palette: optional (for CLUT textures)
 */

#include "loader/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GL/gl.h>

/* TIM2 magic number */
#define TIM2_MAGIC 0x324D4954

/* Texture compression types */
#define TIM2_TFMT_4BIT   0  /* 4bpp indexed (16 colors) */
#define TIM2_TFMT_8BIT   1  /* 8bpp indexed (256 colors) */
#define TIM2_TFMT_16BIT  2  /* 16bpp RGB5A1 */
#define TIM2_TFMT_24BIT  3  /* 24bpp RGB8 */
#define TIM2_TFMT_32BIT  4  /* 32bpp RGBA8 */

/* ============================================
 * Внутренние функции
 * ============================================ */

/* Read 32-bit little-endian value */
static uint32_t read_u32(const uint8_t* data, uint32_t offset) {
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset+1] << 8) |
           ((uint32_t)data[offset+2] << 16) |
           ((uint32_t)data[offset+3] << 24);
}

/* Read 16-bit little-endian value */
static uint16_t read_u16(const uint8_t* data, uint32_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset+1] << 8);
}

/* Convert TIM2 8bpp indexed to RGBA32 */
static void convert_8bpp(const uint8_t* tim_data, uint8_t* rgba, 
                         uint32_t width, uint32_t height, 
                         uint32_t palette_offset) {
    /* Palette is at palette_offset, 256 entries of 4 bytes each (RGBA) */
    const uint8_t* palette = tim_data + palette_offset;
    
    /* Image data follows header */
    const uint8_t* image = tim_data + 20;
    
    uint32_t row_size = width / 2; /* 8bpp: 2 pixels per byte */
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x += 2) {
            uint8_t byte = image[y * row_size + x / 2];
            
            /* Upper nibble = pixel at (x, y) */
            uint8_t idx0 = (byte >> 4) & 0x0F;
            /* Lower nibble = pixel at (x+1, y) */
            uint8_t idx1 = byte & 0x0F;
            
            /* For 4bpp, palette has 16 entries */
            /* Each palette entry is 4 bytes: B, G, R, A */
            uint8_t* col0 = palette + (idx0 * 4);
            uint8_t* col1 = palette + (idx1 * 4);
            
            rgba[(y * width + x) * 4 + 0] = col0[2]; /* R */
            rgba[(y * width + x) * 4 + 1] = col0[1]; /* G */
            rgba[(y * width + x) * 4 + 2] = col0[0]; /* B */
            rgba[(y * width + x) * 4 + 3] = col0[3]; /* A */
            
            if (x + 1 < width) {
                rgba[(y * width + x + 1) * 4 + 0] = col1[2];
                rgba[(y * width + x + 1) * 4 + 1] = col1[1];
                rgba[(y * width + x + 1) * 4 + 2] = col1[0];
                rgba[(y * width + x + 1) * 4 + 3] = col1[3];
            }
        }
    }
}

/* Convert TIM2 4bpp indexed to RGBA32 */
static void convert_4bpp(const uint8_t* tim_data, uint8_t* rgba,
                         uint32_t width, uint32_t height,
                         uint32_t palette_offset) {
    const uint8_t* palette = tim_data + palette_offset;
    const uint8_t* image = tim_data + 20;
    
    uint32_t row_size = width / 2;
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x += 2) {
            uint8_t byte = image[y * row_size + x / 2];
            
            uint8_t idx0 = (byte >> 4) & 0x0F;
            uint8_t idx1 = byte & 0x0F;
            
            uint8_t* col0 = palette + (idx0 * 4);
            uint8_t* col1 = palette + (idx1 * 4);
            
            rgba[(y * width + x) * 4 + 0] = col0[2];
            rgba[(y * width + x) * 4 + 1] = col0[1];
            rgba[(y * width + x) * 4 + 2] = col0[0];
            rgba[(y * width + x) * 4 + 3] = col0[3];
            
            if (x + 1 < width) {
                rgba[(y * width + x + 1) * 4 + 0] = col1[2];
                rgba[(y * width + x + 1) * 4 + 1] = col1[1];
                rgba[(y * width + x + 1) * 4 + 2] = col1[0];
                rgba[(y * width + x + 1) * 4 + 3] = col1[3];
            }
        }
    }
}

/* ============================================
 * Загрузка TIM2 текстуры
 * ============================================ */

TIM2Texture* TIM2_Load(const char* filepath) {
    if (!filepath) return NULL;
    
    /* Read file */
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("ERROR: Cannot open TIM2 file: %s\n", filepath);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = (uint8_t*)malloc(file_size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    fread(data, 1, file_size, f);
    fclose(f);
    
    /* Parse header */
    TIM2Header* header = (TIM2Header*)data;
    
    if (header->magic != TIM2_MAGIC) {
        printf("ERROR: Invalid TIM2 magic: 0x%08X (expected 0x%08X)\n", 
               header->magic, TIM2_MAGIC);
        free(data);
        return NULL;
    }
    
    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t flags = header->flags;
    uint32_t palette_offset = header->palette;
    
    /* Determine compression format */
    uint32_t format = flags & 0x07;
    
    /* Create texture */
    TIM2Texture* tex = (TIM2Texture*)calloc(1, sizeof(TIM2Texture));
    if (!tex) {
        free(data);
        return NULL;
    }
    
    tex->width = width;
    tex->height = height;
    tex->data = data;
    tex->data_size = (uint32_t)file_size;
    
    printf("TIM2: Loaded %s (%ux%u, format=%u)\n", filepath, width, height, format);
    
    return tex;
}

/* ============================================
 * Конвертация в OpenGL текстуру
 * ============================================ */

void TIM2_ToOpenGL(TIM2Texture* tex) {
    if (!tex) return;
    
    TIM2Header* header = (TIM2Header*)tex->data;
    uint32_t format = header->flags & 0x07;
    
    /* Convert to RGBA32 */
    uint8_t* rgba = (uint8_t*)calloc(tex->width * tex->height * 4, 1);
    if (!rgba) return;
    
    if (format == TIM2_TFMT_4BIT || format == TIM2_TFMT_8BIT) {
        if (format == TIM2_TFMT_8BIT) {
            convert_8bpp(tex->data, rgba, tex->width, tex->height, header->palette);
        } else {
            convert_4bpp(tex->data, rgba, tex->width, tex->height, header->palette);
        }
    } else {
        /* For now, just copy raw data */
        memcpy(rgba, tex->data + 20, tex->width * tex->height * 4);
    }
    
    /* Create OpenGL texture */
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 tex->width, tex->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    
    /* Set texture parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    free(rgba);
    
    printf("TIM2: OpenGL texture created: ID=%u, %ux%u\n", tex->id, tex->width, tex->height);
}

/* ============================================
 * Уничтожение текстуры
 * ============================================ */

void TIM2_Free(TIM2Texture* tex) {
    if (!tex) return;
    
    if (tex->id) {
        glDeleteTextures(1, &tex->id);
    }
    
    if (tex->data) {
        free(tex->data);
    }
    
    free(tex);
}
