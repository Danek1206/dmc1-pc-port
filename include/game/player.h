/*
 * DMC1 PC Port - Player System
 * Система игрока (Данте)
 * 
 * Поддерживает:
 *   - Движение (WASD)
 *   - Атаку (ЛКМ)
 *   - Прыжок (Пробел)
 *   - Додж (Shift)
 *   - Блок (R)
 *   - Слоты оружия (Q/E)
 *   - Devil Trigger
 *   - Систему стилей (Swordmaster, Gunslinger, etc.)
 */

#ifndef DMC1_PLAYER_H
#define DMC1_PLAYER_H

#include <stdint.h>

/* Player states */
typedef enum {
    PLAYER_STATE_IDLE,
    PLAYER_STATE_WALK,
    PLAYER_STATE_RUN,
    PLAYER_STATE_JUMP,
    PLAYER_STATE_FALL,
    PLAYER_STATE_ATTACK,
    PLAYER_STATE_DODGE,
    PLAYER_STATE_BLOCK,
    PLAYER_STATE_HIT,
    PLAYER_STATE_DEVIL_TRIGGER,
    PLAYER_STATE_DEAD,
} PlayerStateType;

/* Weapon types */
typedef enum {
    WEAPON_NONE,
    WEAPON_SWORD,       /* Rebellion */
    WEAPON_GUN,         /* Ebony & Ivory */
    WEAPON_CLAW,        /* Agni & Rudra */
    WEAPON_AXE,         /* Star of David */
    WEAPON_WHIP,        /* Sword Breaker */
    WEAPON_DUAL_SWORDS, /* Force Edge */
    WEAPON_HEAVY,       /* Gauntlet */
    WEAPON_MAX
} WeaponType;

/* Style types */
typedef enum {
    STYLE_NONE,
    STYLE_SWORDMASTER,
    STYLE_GUNSLINGER,
    STYLE_TRICKSTER,
    STYLE_QUICKSURVIVOR,
    STYLE_MAX
} StyleType;

/* Player data */
typedef struct {
    /* Position and rotation */
    float pos_x, pos_y, pos_z;
    float rot_y;  /* Поворот по горизонтали */
    
    /* Velocity */
    float vel_x, vel_y, vel_z;
    
    /* State */
    PlayerStateType state;
    StyleType style;
    WeaponType current_weapon;
    
    /* Health and stats */
    int health;
    int max_health;
    int devil_gauge;
    int max_devil_gauge;
    int score;
    int combo_count;
    
    /* Movement */
    float speed;
    float jump_force;
    int is_grounded;
    
    /* Animation */
    int anim_frame;
    float anim_timer;
    
    /* Combat */
    int attack_timer;
    int dodge_timer;
    int block_timer;
    int hit_timer;
} PlayerData;

/* ============================================
 * Создание игрока
 * ============================================ */

PlayerData* Player_Create(void);

/* Обновление игрока */
void Player_Update(PlayerData* player, void* input, double dt);

/* Рендеринг игрока */
void Player_Render(PlayerData* player);

/* Уничтожение игрока */
void Player_Destroy(PlayerData* player);

/* Сеттеры */
void Player_SetPosition(PlayerData* player, float x, float y, float z);
void Player_SetRotation(PlayerData* player, float rot_y);
void Player_SetWeapon(PlayerData* player, WeaponType weapon);
void Player_SetStyle(PlayerData* player, StyleType style);
void Player_Heal(PlayerData* player, int amount);
void Player_TakeDamage(PlayerData* player, int damage);
void Player_ChangeDevilGauge(PlayerData* player, int amount);

#endif /* DMC1_PLAYER_H */
