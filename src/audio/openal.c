/*
 * DMC1 PC Port - Audio System (OpenAL / Stub)
 * Замена PS2 SPU2 (Sound Processing Unit)
 * 
 * Если OpenAL недоступен — используются заглушки.
 */

#include "audio/openal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Создание аудио системы
 * ============================================ */

OpenALAudio* OpenAL_Create(void) {
    /* Audio is optional — return NULL to indicate no audio */
    printf("WARNING: OpenAL not available (continuing without sound)\n");
    return NULL;
}

/* ============================================
 * Звуки (заглушки)
 * ============================================ */

int OpenAL_LoadSound(OpenALAudio* audio, const char* filepath) {
    (void)audio;
    (void)filepath;
    return -1;
}

void OpenAL_PlaySound(OpenALAudio* audio, int sound_id, int loop) {
    (void)audio;
    (void)sound_id;
    (void)loop;
}

/* ============================================
 * 3D позиционный звук (заглушки)
 * ============================================ */

void OpenAL_SetListenerPos(OpenALAudio* audio, float x, float y, float z) {
    (void)audio; (void)x; (void)y; (void)z;
}

void OpenAL_SetListenerVel(OpenALAudio* audio, float x, float y, float z) {
    (void)audio; (void)x; (void)y; (void)z;
}

void OpenAL_SetSourcePos(OpenALAudio* audio, int source_id, float x, float y, float z) {
    (void)audio; (void)source_id; (void)x; (void)y; (void)z;
}

/* ============================================
 * Музыка (заглушки)
 * ============================================ */

void OpenAL_PlayMusic(OpenALAudio* audio, const char* filepath) {
    (void)audio; (void)filepath;
}

void OpenAL_SetMusicVolume(OpenALAudio* audio, float volume) {
    (void)audio; (void)volume;
}

/* ============================================
 * Обновление аудио (заглушка)
 * ============================================ */

void OpenAL_Update(OpenALAudio* audio) {
    (void)audio;
}

/* ============================================
 * Уничтожение
 * ============================================ */

void OpenAL_Destroy(OpenALAudio* audio) {
    free(audio);
}
