/*
 * DMC1 PC Port - World System
 * 3D мир игры
 * 
 * Поддерживает:
 *   - Загрузку уровней (FSD файлы PS2)
 *   - Рендеринг геометрии
 *   - Текстуры
 *   - Освещение
 *   - Враги и NPC
 *   - Физика (простая)
 */

#ifndef DMC1_WORLD_H
#define DMC1_WORLD_H

#include <stdint.h>

/* Maximum entities */
#define MAX_ENTITIES 256

/* Entity types */
typedef enum {
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_PROJECTILE,
    ENTITY_ITEM,
    ENTITY_PROP,
    ENTITY_LIGHT,
} EntityType;

/* Entity */
typedef struct {
    EntityType  type;
    int         id;
    int         active;
    
    /* Transform */
    float pos_x, pos_y, pos_z;
    float rot_y;
    
    /* Physics */
    float vel_x, vel_y, vel_z;
    
    /* Render */
    int model_id;
    int texture_id;
} WorldEntity;

/* World */
typedef struct {
    /* Entities */
    WorldEntity entities[MAX_ENTITIES];
    int entity_count;
    
    /* Level data */
    char level_name[256];
    int stage_number;
    
    /* Lighting */
    float ambient_r, ambient_g, ambient_b;
    float light_dir_x, light_dir_y, light_dir_z;
    
    /* Fog */
    int fog_enabled;
    float fog_r, fog_g, fog_b, fog_density;
} WorldData;

/* Create world */
WorldData* World_Create(void);

/* Update world */
void World_Update(WorldData* world, double dt);

/* Render world */
void World_Render(WorldData* world);

/* Entity management */
int  World_AddEntity(WorldData* world, EntityType type);
void World_RemoveEntity(WorldData* world, int entity_id);
void World_SetEntityPos(WorldData* world, int entity_id, float x, float y, float z);

/* Level loading */
int World_LoadLevel(WorldData* world, const char* filepath);

/* Destroy world */
void World_Destroy(WorldData* world);

#endif /* DMC1_WORLD_H */
