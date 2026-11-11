/*
 * DMC1 PC Port - FSD Level Loader
 * Загрузка PS2 FSD уровней/сцен
 * 
 * FSD формат:
 * - No magic header
 * - Contains BSP tree nodes
 * - Contains vertex/polygon data
 */

#include "loader/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * Загрузка FSD уровня
 * ============================================ */

FSDLevel* FSD_Load(const char* filepath) {
    if (!filepath) return NULL;
    
    /* Read file */
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("ERROR: Cannot open FSD file: %s\n", filepath);
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
    
    /* Parse level name from filename */
    const char* name = strrchr(filepath, '\\');
    if (name) name++;
    else name = filepath;
    
    printf("FSD: Loaded %s (%ld bytes)\n", name, file_size);
    
    /* Create level */
    FSDLevel* level = (FSDLevel*)calloc(1, sizeof(FSDLevel));
    if (!level) {
        free(data);
        return NULL;
    }
    
    strncpy(level->name, name, 63);
    level->name[63] = '\0';
    
    /* TODO: Parse BSP tree and geometry from data */
    /* For now, store raw data reference */
    level->nodes = NULL;
    level->node_count = 0;
    level->vertices = NULL;
    level->vertex_count = 0;
    level->polygons = NULL;
    level->polygon_count = 0;
    
    /* Default fog settings */
    level->fog_r = 0.1f;
    level->fog_g = 0.1f;
    level->fog_b = 0.2f;
    level->fog_density = 0.01f;
    
    free(data);
    
    return level;
}

/* ============================================
 * Освобождение уровня
 * ============================================ */

void FSD_Free(FSDLevel* level) {
    if (!level) return;
    
    if (level->nodes) free(level->nodes);
    if (level->vertices) free(level->vertices);
    if (level->polygons) free(level->polygons);
    
    free(level);
}
