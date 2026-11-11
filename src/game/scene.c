/*
 * DMC1 PC Port - Scene System Implementation
 * Система сцен (меню, игра, кат-сцены)
 */

#include "game/scene.h"
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Создание сцены
 * ============================================ */

SceneData* Scene_Create(void) {
    SceneData* scene = (SceneData*)calloc(1, sizeof(SceneData));
    if (!scene) return NULL;
    
    /* Начинаем с экрана загрузки */
    scene->current = SCENE_LOADING;
    scene->loading_progress = 0;
    strncpy(scene->loading_text, "Loading Devil May Cry...", 255);
    scene->loading_text[255] = '\0';
    
    /* HUD */
    scene->show_hud = 1;
    scene->show_combobox = 1;
    scene->show_devilgauge = 1;
    
    return scene;
}

/* ============================================
 * Обновление сцены
 * ============================================ */

void Scene_Update(SceneData* scene, double dt) {
    if (!scene) return;
    
    /* Обновляем загрузку */
    if (scene->current == SCENE_LOADING) {
        scene->loading_progress += (int)(dt * 50); /* ~2 секунды */
        if (scene->loading_progress >= 100) {
            scene->loading_progress = 100;
            Scene_Change(scene, SCENE_GAME);
        }
    }
    
    /* Обработка паузы */
    if (scene->current == SCENE_GAME) {
        /* TODO: Проверить нажатие Enter для паузы */
    }
    
    /* Обновляем кат-сцену */
    if (scene->current == SCENE_CUTSCENE) {
        scene->cutscene_frame++;
        /* TODO: Воспроизвести кат-сцену */
    }
}

/* ============================================
 * Рендеринг UI
 * ============================================ */

void Scene_RenderUI(SceneData* scene) {
    if (!scene) return;
    
    /* TODO: Полноценный UI с текстурами */
    /* TODO: HUD (здоровье, devil gauge, оружие) */
    /* TODO: Комбо-бокс */
    /* TODO: Меню паузы */
    /* TODO: Экран загрузки */
    
    switch (scene->current) {
        case SCENE_LOADING:
            /* Рисуем прогресс загрузки */
            /* TODO: glDrawText или подобная функция */
            break;
            
        case SCENE_GAME:
            if (scene->show_hud) {
                /* Рисуем HUD: здоровье, devil gauge, оружие */
                /* TODO: OpenGL 2D рендеринг */
            }
            break;
            
        case SCENE_PAUSE:
            /* Рисуем меню паузы */
            /* TODO: OpenGL 2D рендеринг */
            break;
            
        case SCENE_TITLE:
            /* Рисуем главное меню */
            /* TODO: OpenGL 2D рендеринг */
            break;
            
        default:
            break;
    }
}

/* ============================================
 * Смена сцены
 * ============================================ */

void Scene_Change(SceneData* scene, SceneType new_scene) {
    if (!scene) return;
    
    scene->previous = scene->current;
    scene->current = new_scene;
    
    /* Сбрасываем состояние новой сцены */
    switch (new_scene) {
        case SCENE_LOADING:
            scene->loading_progress = 0;
            break;
        case SCENE_GAME:
            scene->show_hud = 1;
            break;
        case SCENE_TITLE:
            /* TODO: Загрузить текстуры меню */
            break;
        default:
            break;
    }
}

/* ============================================
 * Уничтожение
 * ============================================ */

void Scene_Destroy(SceneData* scene) {
    if (scene) free(scene);
}
