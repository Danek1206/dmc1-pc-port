/*
 * DMC1 PC Port - Input System
 * Управление в стиле DMC 4/5:
 *   WASD - движение
 *   ЛКМ  - атака
 *   Мышь - камера
 *   Пробел - прыжок
 *   Shift - додж
 *   R - блок
 *   Q/E - слоты оружия
 */

#ifndef DMC1_GAMEPAD_H
#define DMC1_GAMEPAD_H

#include <stdint.h>

/* Button flags */
#define INPUT_ATTACK    0x0001  /* ЛКМ - Square */
#define INPUT_JUMP      0x0002  /* Пробел - Cross */
#define INPUT_DODGE     0x0004  /* Shift - Circle */
#define INPUT_BLOCK     0x0008  /* R - Triangle */
#define INPUT_SLOT_UP   0x0010  /* Q - L1 */
#define INPUT_SLOT_DOWN 0x0020  /* E - R1 */
#define INPUT_MENU      0x0040  /* Enter - Start */
#define INPUT_INVENTORY 0x0080  /* Tab - Select */
#define INPUT_SPRINT    0x0100  /* Ctrl - R2 */

/* Movement direction */
typedef struct {
    float x;  /* -1.0 (A) to 1.0 (D) */
    float y;  /* -1.0 (W) to 1.0 (S) */
} InputVec2;

/* Camera rotation from mouse */
typedef struct {
    float yaw;    /* горизонтальная камера */
    float pitch;  /* вертикальная камера */
    float delta_x; /* смещение мыши X */
    float delta_y; /* смещение мыши Y */
} InputCamera;

/* Full input state */
typedef struct GamepadState GamepadState;

/* Initialize input system */
GamepadState* Gamepad_Init(void);

/* Update input state with Win32 raw input */
void Gamepad_Update(GamepadState* state, const void* win32);

/* Destroy input system */
void Gamepad_Destroy(GamepadState* state);

#endif /* DMC1_GAMEPAD_H */
