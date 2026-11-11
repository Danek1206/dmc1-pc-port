/*
 * DMC1 PC Port - Player System Implementation
 * Система игрока (Данте)
 */

#include "game/player.h"
#include "input/gamepad.h"
#include <stdlib.h>
#include <math.h>

/* ============================================
 * Создание игрока
 * ============================================ */

PlayerData* Player_Create(void) {
    PlayerData* player = (PlayerData*)calloc(1, sizeof(PlayerData));
    if (!player) return NULL;
    
    /* Начальная позиция */
    player->pos_x = 0.0f;
    player->pos_y = 0.0f;
    player->pos_z = 0.0f;
    player->rot_y = 0.0f;
    
    /* Начальное состояние */
    player->state = PLAYER_STATE_IDLE;
    player->style = STYLE_SWORDMASTER;
    player->current_weapon = WEAPON_SWORD;
    
    /* Здоровье */
    player->health = 100;
    player->max_health = 100;
    
    /* Devil Gauge */
    player->devil_gauge = 0;
    player->max_devil_gauge = 100;
    
    /* Скорость */
    player->speed = 3.0f;
    player->jump_force = 8.0f;
    player->is_grounded = 1;
    
    /* Оружие */
    player->attack_timer = 0;
    player->dodge_timer = 0;
    player->block_timer = 0;
    player->hit_timer = 0;
    
    return player;
}

/* ============================================
 * Обновление игрока
 * ============================================ */

void Player_Update(PlayerData* player, void* input_ptr, double dt) {
    if (!player || !input_ptr) return;
    
    GamepadState* input = (GamepadState*)input_ptr;
    
    /* --- Движение --- */
    float move_speed = player->speed;
    
    /* Проверка спринта */
    if (input->buttons & INPUT_SPRINT) {
        move_speed *= 1.8f;
    }
    
    /* Применяем движение */
    float dx = input->move_dir.x * move_speed * (float)dt;
    float dy = input->move_dir.y * move_speed * (float)dt;
    
    /* Движение относительно поворота камеры */
    float cos_y = cosf(player->rot_y);
    float sin_y = sinf(player->rot_y);
    
    player->pos_x += (dx * sin_y + dy * cos_y);
    player->pos_z += (dx * cos_y - dy * sin_y);
    
    /* --- Прыжок --- */
    if ((input->buttons & INPUT_JUMP) && player->is_grounded) {
        player->vel_y = player->jump_force;
        player->is_grounded = 0;
        player->state = PLAYER_STATE_JUMP;
    }
    
    /* Гравитация */
    if (!player->is_grounded) {
        player->vel_y -= 20.0f * (float)dt; /* g = 20 m/s² */
        player->pos_y += player->vel_y * (float)dt;
        
        if (player->pos_y <= 0.0f) {
            player->pos_y = 0.0f;
            player->vel_y = 0.0f;
            player->is_grounded = 1;
            player->state = PLAYER_STATE_IDLE;
        }
    }
    
    /* --- Атака --- */
    if (input->buttons & INPUT_ATTACK) {
        if (player->attack_timer <= 0) {
            player->state = PLAYER_STATE_ATTACK;
            player->attack_timer = 15; /* 15 кадров */
            player->devil_gauge = fmin(player->devil_gauge + 2, player->max_devil_gauge);
        }
    }
    if (player->attack_timer > 0) player->attack_timer--;
    if (player->attack_timer <= 0 && player->state == PLAYER_STATE_ATTACK) {
        player->state = PLAYER_STATE_IDLE;
    }
    
    /* --- Додж --- */
    if (input->buttons & INPUT_DODGE) {
        if (player->dodge_timer <= 0) {
            player->state = PLAYER_STATE_DODGE;
            player->dodge_timer = 20;
        }
    }
    if (player->dodge_timer > 0) {
        player->dodge_timer--;
        if (player->dodge_timer <= 0 && player->state == PLAYER_STATE_DODGE) {
            player->state = PLAYER_STATE_IDLE;
        }
    }
    
    /* --- Блок --- */
    if (input->buttons & INPUT_BLOCK) {
        player->state = PLAYER_STATE_BLOCK;
    } else if (player->state == PLAYER_STATE_BLOCK) {
        player->state = PLAYER_STATE_IDLE;
    }
    
    /* --- Смена оружия --- */
    if (input->buttons & INPUT_SLOT_UP) {
        /* Смена слота оружия -1 */
        input->buttons &= ~INPUT_SLOT_UP; /* Debounce */
    }
    if (input->buttons & INPUT_SLOT_DOWN) {
        /* Смена слота оружия +1 */
        input->buttons &= ~INPUT_SLOT_DOWN; /* Debounce */
    }
    
    /* --- Devil Trigger --- */
    if (player->devil_gauge >= player->max_devil_gauge) {
        /* Можно активировать Devil Trigger (нажата комбинация) */
    }
    
    /* --- Комбо --- */
    if (player->attack_timer > 0 && player->state == PLAYER_STATE_ATTACK) {
        player->combo_count++;
    }
    
    /* --- Обновление анимации --- */
    player->anim_timer += dt;
    if (player->anim_timer > 0.05) {
        player->anim_frame++;
        player->anim_timer = 0.0f;
    }
}

/* ============================================
 * Рендеринг игрока
 * ============================================ */

void Player_Render(PlayerData* player) {
    if (!player) return;
    
    /* TODO: Загрузить и отрендерить 3D модель Данте */
    /* TODO: Загрузить и отрендерить оружие */
    /* TODO: Применить анимацию */
    /* TODO: Отрисовать эффекты (Devil Trigger, комбо) */
    
    (void)player;
}

/* ============================================
 * Сеттеры
 * ============================================ */

void Player_SetPosition(PlayerData* player, float x, float y, float z) {
    if (!player) return;
    player->pos_x = x;
    player->pos_y = y;
    player->pos_z = z;
}

void Player_SetRotation(PlayerData* player, float rot_y) {
    if (!player) return;
    player->rot_y = rot_y;
}

void Player_SetWeapon(PlayerData* player, WeaponType weapon) {
    if (!player) return;
    player->current_weapon = weapon;
}

void Player_SetStyle(PlayerData* player, StyleType style) {
    if (!player) return;
    player->style = style;
}

void Player_Heal(PlayerData* player, int amount) {
    if (!player) return;
    player->health = fmin(player->health + amount, player->max_health);
}

void Player_TakeDamage(PlayerData* player, int damage) {
    if (!player) return;
    player->health -= damage;
    if (player->health <= 0) {
        player->health = 0;
        player->state = PLAYER_STATE_DEAD;
    }
}

void Player_ChangeDevilGauge(PlayerData* player, int amount) {
    if (!player) return;
    player->devil_gauge = fmax(0, fmin(player->devil_gauge + amount, player->max_devil_gauge));
}

/* ============================================
 * Уничтожение
 * ============================================ */

void Player_Destroy(PlayerData* player) {
    if (player) free(player);
}
