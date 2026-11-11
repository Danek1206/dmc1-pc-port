/*
 * DMC1 PC Port - Camera System Implementation
 * Система камеры в стиле DMC 4/5
 */

#include "game/camera.h"
#include "input/gamepad.h"
#include <math.h>

#define PI 3.14159265f

/* ============================================
 * Создание камеры
 * ============================================ */

CameraData* Camera_Create(void) {
    CameraData* camera = (CameraData*)calloc(1, sizeof(CameraData));
    if (!camera) return NULL;
    
    /* Начальные параметры */
    camera->distance = 3.0f;
    camera->min_distance = 1.5f;
    camera->max_distance = 6.0f;
    camera->zoom = 1.0f;
    
    /* Смещение камеры относительно игрока */
    camera->offset_x = 0.0f;
    camera->offset_y = 1.5f;  /* Чуть выше уровня глаз */
    camera->offset_z = -3.0f; /* Сзади игрока */
    
    /* Collision avoidance */
    camera->collision_enabled = 1;
    
    return camera;
}

/* ============================================
 * Обновление камеры
 * ============================================ */

void Camera_Update(CameraData* camera, void* player_ptr, void* input_ptr, double dt) {
    if (!camera) return;
    
    /* Получаем данные игрока */
    /* В полной реализации: PlayerData* player = (PlayerData*)player_ptr; */
    (void)player_ptr;
    (void)dt;
    
    /* Получаем данные ввода */
    GamepadState* input = (GamepadState*)input_ptr;
    
    /* Обновляем yaw/pitch от мыши */
    if (input) {
        camera->yaw += input->camera.delta_x;
        camera->pitch += input->camera.delta_y;
        
        /* Ограничиваем pitch */
        float pitch_deg = camera->pitch * (180.0f / PI);
        if (pitch_deg > 60.0f) pitch_deg = 60.0f;
        if (pitch_deg < -30.0f) pitch_deg = -30.0f;
        camera->pitch = pitch_deg * (PI / 180.0f);
    }
    
    /* Вычисляем позицию камеры */
    float cos_yaw = cosf(camera->yaw);
    float sin_yaw = sinf(camera->yaw);
    float cos_pitch = cosf(camera->pitch);
    float sin_pitch = sinf(camera->pitch);
    
    /* Позиция камеры относительно цели */
    float dist = camera->distance * camera->zoom;
    
    camera->pos_x = camera->target_x + dist * sin_yaw * cos_pitch;
    camera->pos_y = camera->target_y + dist * sin_pitch + camera->offset_y;
    camera->pos_z = camera->target_z + dist * cos_yaw * cos_pitch;
    
    /* Collision avoidance (raycast между камерой и игроком) */
    if (camera->collision_enabled) {
        /* TODO: Реализовать raycast для проверки препятствий */
        /* Если между камерой и игроком есть стена — отодвинуть камеру */
    }
}

/* ============================================
 * Настройка OpenGL камеры
 * ============================================ */

void Camera_SetupOpenGL(CameraData* camera) {
    if (!camera) return;
    
    /* Устанавливаем матрицу вида */
    float forward_x = camera->target_x - camera->pos_x;
    float forward_y = camera->target_y - camera->pos_y;
    float forward_z = camera->target_z - camera->pos_z;
    
    /* Нормализуем forward */
    float len = sqrtf(forward_x * forward_x + forward_y * forward_y + forward_z * forward_z);
    if (len > 0.001f) {
        forward_x /= len;
        forward_y /= len;
        forward_z /= len;
    }
    
    /* Right = cross(forward, up) */
    float right_x = forward_z;  /* up = (0,1,0) */
    float right_y = 0.0f;
    float right_z = -forward_x;
    len = sqrtf(right_x * right_x + right_z * right_z);
    if (len > 0.001f) {
        right_x /= len;
        right_z /= len;
    }
    
    /* Up = cross(right, forward) */
    float up_x = right_y * forward_z - right_z * forward_y;
    float up_y = right_z * forward_x - right_x * forward_z;
    float up_z = right_x * forward_y - right_y * forward_x;
    
    /* Строим матрицу вида */
    float view_matrix[16];
    view_matrix[0]  = right_x;  view_matrix[1]  = right_y;  view_matrix[2]  = right_z;  view_matrix[3]  = 0.0f;
    view_matrix[4]  = up_x;     view_matrix[5]  = up_y;     view_matrix[6]  = up_z;     view_matrix[7]  = 0.0f;
    view_matrix[8]  = -forward_x; view_matrix[9] = -forward_y; view_matrix[10] = -forward_z; view_matrix[11] = 0.0f;
    view_matrix[12] = 0.0f;     view_matrix[13] = 0.0f;     view_matrix[14] = 0.0f;     view_matrix[15] = 1.0f;
    
    /* Загружаем матрицу */
    glLoadMatrixf(view_matrix);
    
    /* Устанавливаем проекцию */
    glMatrixMode(GL_PROJECTION);
    float fov = 60.0f * (PI / 180.0f);
    float aspect = (float)1280.0f / 720.0f;
    float f = 1.0f / tanf(fov / 2.0f);
    
    float proj_matrix[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (1.0f + 100.0f) / (1.0f - 100.0f), -1.0f,
        0, 0, (2.0f * 100.0f) / (1.0f - 100.0f), 0.0f
    };
    glLoadMatrixf(proj_matrix);
    glMatrixMode(GL_MODELVIEW);
}

/* ============================================
 * Уничтожение
 * ============================================ */

void Camera_Destroy(CameraData* camera) {
    if (camera) free(camera);
}
