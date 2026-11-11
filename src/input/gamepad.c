/*
 * DMC1 PC Port - Input System Implementation
 * Маппинг клавиатуры/мыши на структуру ввода
 */

#include "input/gamepad.h"
#include "sys/win32.h"
#include <stdlib.h>
#include <math.h>

/* Клавиши (Virtual-Key codes) */
#define KEY_W       0x57
#define KEY_A       0x41
#define KEY_S       0x53
#define KEY_D       0x44
#define KEY_SPACE   0x20
#define KEY_SHIFT   0xA0
#define KEY_R       0x52
#define KEY_Q       0x51
#define KEY_E       0x45
#define KEY_ENTER   0x0D
#define KEY_TAB     0x09
#define KEY_RMOUSE  0xA2
#define KEY_LMOUSE  0xA1

/* Internal gamepad state */
struct GamepadState {
    /* Movement */
    InputVec2 move_dir;
    
    /* Camera */
    InputCamera camera;
    
    /* Buttons */
    uint16_t buttons;
    
    /* Sensitivity */
    float mouse_sensitivity;
    
    /* Raw mouse position for delta calculation */
    int last_mouse_x;
    int last_mouse_y;
};

/* ============================================
 * Инициализация
 * ============================================ */

GamepadState* Gamepad_Init(void) {
    GamepadState* state = (GamepadState*)calloc(1, sizeof(GamepadState));
    if (!state) return NULL;
    
    state->mouse_sensitivity = 0.002f;
    state->last_mouse_x = 0;
    state->last_mouse_y = 0;
    
    return state;
}

/* ============================================
 * Обновление ввода
 * ============================================ */

void Gamepad_Update(GamepadState* state, const void* win32_ptr) {
    if (!state || !win32_ptr) return;
    
    const Win32State* win32 = (const Win32State*)win32_ptr;
    
    /* --- Движение (WASD) --- */
    state->move_dir.x = 0.0f;
    state->move_dir.y = 0.0f;
    
    /* W = вверх (вперёд) */
    if (win32->keys[KEY_W]) state->move_dir.y = -1.0f;
    /* S = вниз (назад) */
    if (win32->keys[KEY_S]) state->move_dir.y = 1.0f;
    /* A = влево */
    if (win32->keys[KEY_A]) state->move_dir.x = -1.0f;
    /* D = вправо */
    if (win32->keys[KEY_D]) state->move_dir.x = 1.0f;
    
    /* Нормализация диагонального движения */
    if (state->move_dir.x != 0.0f && state->move_dir.y != 0.0f) {
        float len = sqrtf(state->move_dir.x * state->move_dir.x + 
                         state->move_dir.y * state->move_dir.y);
        state->move_dir.x /= len;
        state->move_dir.y /= len;
    }
    
    /* --- Камера (мышь) --- */
    state->camera.delta_x = (float)(state->last_mouse_x - win32->mouse_x);
    state->camera.delta_y = (float)(state->last_mouse_y - win32->mouse_y);
    
    /* Обновляем последнюю позицию мыши */
    state->last_mouse_x = win32->mouse_x;
    state->last_mouse_y = win32->mouse_y;
    
    /* Применяем чувствительность */
    state->camera.delta_x *= state->mouse_sensitivity;
    state->camera.delta_y *= state->mouse_sensitivity;
    
    /* Обновляем углы камеры */
    state->camera.yaw   += state->camera.delta_x;
    state->camera.pitch += state->camera.delta_y;
    
    /* Ограничиваем вертикальный угол (-85 до +85 градусов) */
    float pitch_deg = state->camera.pitch * (180.0f / 3.14159265f);
    if (pitch_deg > 85.0f) pitch_deg = 85.0f;
    if (pitch_deg < -85.0f) pitch_deg = -85.0f;
    state->camera.pitch = pitch_deg * (3.14159265f / 180.0f);
    
    /* --- Кнопки --- */
    state->buttons = 0;
    
    /* ЛКМ -> Атака (Square) */
    if (win32->mouse_buttons & MOUSE_LEFT)
        state->buttons |= INPUT_ATTACK;
    
    /* Пробел -> Прыжок (Cross) */
    if (win32->keys[KEY_SPACE])
        state->buttons |= INPUT_JUMP;
    
    /* Shift -> Додж (Circle) */
    if (win32->keys[KEY_SHIFT])
        state->buttons |= INPUT_DODGE;
    
    /* R -> Блок (Triangle) */
    if (win32->keys[KEY_R])
        state->buttons |= INPUT_BLOCK;
    
    /* Q -> Слот оружия -1 (L1) */
    if (win32->keys[KEY_Q])
        state->buttons |= INPUT_SLOT_UP;
    
    /* E -> Слот оружия +1 (R1) */
    if (win32->keys[KEY_E])
        state->buttons |= INPUT_SLOT_DOWN;
    
    /* Enter -> Меню (Start) */
    if (win32->keys[KEY_ENTER])
        state->buttons |= INPUT_MENU;
    
    /* Tab -> Инвентарь (Select) */
    if (win32->keys[KEY_TAB])
        state->buttons |= INPUT_INVENTORY;
    
    /* Ctrl -> Бег (R2) */
    if (win32->keys[KEY_RMOUSE] || win32->keys[KEY_LMOUSE])
        state->buttons |= INPUT_SPRINT;
}

/* ============================================
 * Уничтожение
 * ============================================ */

void Gamepad_Destroy(GamepadState* state) {
    if (state) free(state);
}
