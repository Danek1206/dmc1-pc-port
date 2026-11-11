/*
 * DMC1 PC Port - World System Implementation
 * 3D мир игры
 */

#include "game/world.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * Создание мира
 * ============================================ */

WorldData* World_Create(void) {
    WorldData* world = (WorldData*)calloc(1, sizeof(WorldData));
    if (!world) return NULL;
    
    /* Начальное освещение */
    world->ambient_r = 0.3f;
    world->ambient_g = 0.3f;
    world->ambient_b = 0.4f;
    world->light_dir_x = 0.5f;
    world->light_dir_y = 1.0f;
    world->light_dir_z = 0.3f;
    
    /* Fog */
    world->fog_enabled = 1;
    world->fog_r = 0.1f;
    world->fog_g = 0.1f;
    world->fog_b = 0.2f;
    world->fog_density = 0.02f;
    
    return world;
}

/* ============================================
 * Обновление мира
 * ============================================ */

void World_Update(WorldData* world, double dt) {
    if (!world) return;
    
    /* Обновляем физику всех сущностей */
    for (int i = 0; i < world->entity_count; i++) {
        WorldEntity* ent = &world->entities[i];
        if (!ent->active) continue;
        
        /* Применяем скорость */
        ent->pos_x += ent->vel_x * (float)dt;
        ent->pos_y += ent->vel_y * (float)dt;
        ent->pos_z += ent->vel_z * (float)dt;
        
        /* Гравитация для non-флайящих */
        if (ent->type != ENTITY_LIGHT && ent->vel_y > -100.0f) {
            ent->vel_y -= 20.0f * (float)dt;
        }
        
        /* Земля */
        if (ent->pos_y < 0.0f) {
            ent->pos_y = 0.0f;
            ent->vel_y = 0.0f;
        }
    }
}

/* ============================================
 * Рендеринг мира
 * ============================================ */

void World_Render(WorldData* world) {
    if (!world) return;
    
    /* Setup lighting */
    glLightfv(GL_LIGHT0, GL_POSITION, 
              (float[]){world->light_dir_x, world->light_dir_y, world->light_dir_z, 0.0f});
    glLightfv(GL_LIGHT0, GL_AMBIENT, 
              (float[]){world->ambient_r, world->ambient_g, world->ambient_b, 1.0f});
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    
    /* Setup fog */
    if (world->fog_enabled) {
        glEnable(GL_FOG);
        glFogf(GL_FOG_DENSITY, world->fog_density);
        glFogfv(GL_FOG_COLOR, (float[]){world->fog_r, world->fog_g, world->fog_b, 1.0f});
        glFogi(GL_FOG_MODE, GL_EXP);
    }
    
    /* Render all entities */
    for (int i = 0; i < world->entity_count; i++) {
        WorldEntity* ent = &world->entities[i];
        if (!ent->active) continue;
        
        /* TODO: Загрузить и отрендерить модель */
        /* TODO: Применить текстуру */
        /* TODO: Применить анимацию */
        
        (void)ent;
    }
}

/* ============================================
 * Управление сущностями
 * ============================================ */

int World_AddEntity(WorldData* world, EntityType type) {
    if (!world) return -1;
    if (world->entity_count >= MAX_ENTITIES) return -1;
    
    int id = world->entity_count;
    WorldEntity* ent = &world->entities[id];
    
    memset(ent, 0, sizeof(WorldEntity));
    ent->type = type;
    ent->id = id;
    ent->active = 1;
    
    world->entity_count++;
    return id;
}

void World_RemoveEntity(WorldData* world, int entity_id) {
    if (!world || entity_id < 0 || entity_id >= world->entity_count) return;
    
    world->entities[entity_id].active = 0;
}

void World_SetEntityPos(WorldData* world, int entity_id, float x, float y, float z) {
    if (!world || entity_id < 0 || entity_id >= world->entity_count) return;
    
    world->entities[entity_id].pos_x = x;
    world->entities[entity_id].pos_y = y;
    world->entities[entity_id].pos_z = z;
}

/* ============================================
 * Загрузка уровней
 * ============================================ */

int World_LoadLevel(WorldData* world, const char* filepath) {
    if (!world || !filepath) return 0;
    
    /* Загрузка из FSD файла PS2 */
    /* TODO: Парсинг FSD формата */
    /* TODO: Загрузка моделей из PLD/ */
    /* TODO: Загрузка текстур из ETC/ */
    /* TODO: Загрузка событий из ADS/ */
    
    strncpy(world->level_name, filepath, 255);
    world->level_name[255] = '\0';
    
    printf("Loading level: %s\n", filepath);
    return 1;
}

/* ============================================
 * Уничтожение
 * ============================================ */

void World_Destroy(WorldData* world) {
    if (world) free(world);
}
