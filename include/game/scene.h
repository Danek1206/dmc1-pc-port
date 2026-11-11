/*
 * DMC1 PC Port - Scene System
 * Система сцен (меню, игра, кат-сцены)
 * 
 * Поддерживает:
 *   - Главное меню
 *   - Игровой процесс
 *   - Пауза/меню
 *   - Кат-сцены
 *   - Экран загрузки
 *   - UI (здоровье,devil gauge, комбо)
 */

#ifndef DMC1_SCENE_H
#define DMC1_SCENE_H

/* Scene types */
typedef enum {
    SCENE_TITLE,
    SCENE_LOADING,
    SCENE_GAME,
    SCENE_PAUSE,
    SCENE_CUTSCENE,
    SCENE_GAMEOVER,
    SCENE_RANKING,
} SceneType;

/* Scene */
typedef struct {
    SceneType current;
    SceneType previous;
    
    /* Loading */
    int loading_progress;
    char loading_text[256];
    
    /* UI */
    int show_hud;
    int show_combobox;
    int show_devilgauge;
    
    /* Cutscene */
    int cutscene_playing;
    int cutscene_frame;
} SceneData;

/* Create scene */
SceneData* Scene_Create(void);

/* Update scene */
void Scene_Update(SceneData* scene, double dt);

/* Render scene */
void Scene_RenderUI(SceneData* scene);

/* Change scene */
void Scene_Change(SceneData* scene, SceneType new_scene);

/* Destroy scene */
void Scene_Destroy(SceneData* scene);

#endif /* DMC1_SCENE_H */
