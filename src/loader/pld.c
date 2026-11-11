/*
 * DMC1 PC Port - PLD Model Loader
 * Загрузка PS2 PLD 3D моделей
 * 
 * PLD формат:
 * - Header: 4 bytes (sub-model count)
 * - Offset table: 16 entries x 8 bytes = 128 bytes
 * - Each sub-model: vertices + polygons
 */

#include "loader/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* PLD magic/header */
#define PLD_HEADER_SIZE 4
#define PLD_OFFSET_COUNT 16
#define PLD_OFFSET_ENTRY_SIZE 8

/* ============================================
 * Внутренние функции
 * ============================================ */

/* Read 32-bit little-endian */
static uint32_t read_u32(const uint8_t* data, uint32_t offset) {
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset+1] << 8) |
           ((uint32_t)data[offset+2] << 16) |
           ((uint32_t)data[offset+3] << 24);
}

/* Read 16-bit little-endian */
static uint16_t read_u16(const uint8_t* data, uint32_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset+1] << 8);
}

/* ============================================
 * Загрузка PLD модели
 * ============================================ */

PLDModel* PLD_Load(const char* filepath) {
    if (!filepath) return NULL;
    
    /* Read file */
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("ERROR: Cannot open PLD file: %s\n", filepath);
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
    uint32_t sub_model_count = read_u32(data, 0);
    
    printf("PLD: Loaded %s (%u sub-models)\n", filepath, sub_model_count);
    
    /* Create model */
    PLDModel* model = (PLDModel*)calloc(1, sizeof(PLDModel));
    if (!model) {
        free(data);
        return NULL;
    }
    
    model->sub_model_count = sub_model_count;
    model->sub_models = (PLDSubModel*)calloc(sub_model_count, sizeof(PLDSubModel));
    if (!model->sub_models) {
        free(data);
        free(model);
        return NULL;
    }
    
    /* Parse offset table and sub-models */
    for (uint32_t i = 0; i < sub_model_count && i < PLD_OFFSET_COUNT; i++) {
        uint32_t offset = read_u32(data, PLD_HEADER_SIZE + i * PLD_OFFSET_ENTRY_SIZE);
        
        if (offset == 0 || offset >= (uint32_t)file_size) continue;
        
        PLDSubModel* sm = &model->sub_models[i];
        
        /* TODO: Parse actual vertex/polygon data from offset */
        /* For now, create placeholder */
        sm->vertex_count = 0;
        sm->polygon_count = 0;
        sm->vertices = NULL;
        sm->polygons = NULL;
        
        printf("PLD:   Sub-model %u at offset 0x%08X\n", i, offset);
    }
    
    free(data);
    
    return model;
}

/* ============================================
 * Освобождение модели
 * ============================================ */

void PLD_Free(PLDModel* model) {
    if (!model) return;
    
    if (model->sub_models) {
        for (uint32_t i = 0; i < model->sub_model_count; i++) {
            if (model->sub_models[i].vertices) {
                free(model->sub_models[i].vertices);
            }
            if (model->sub_models[i].polygons) {
                free(model->sub_models[i].polygons);
            }
        }
        free(model->sub_models);
    }
    
    free(model);
}
