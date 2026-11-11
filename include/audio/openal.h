/*
 * DMC1 PC Port - OpenAL Audio System
 * Замена PS2 SPU2 (Sound Processing Unit)
 * 
 * Поддерживает:
 *   - 3D позиционный звук
 *   - Музыка (MIDI/MP3)
 *   - Звуковые эффекты
 *   - Загрузка WAV файлов
 */

#ifndef DMC1_OPENAL_H
#define DMC1_OPENAL_H

#include <stddef.h>

typedef struct {
    void* device;
    void* context;
    int   initialized;
} OpenALAudio;

/* Create audio system */
OpenALAudio* OpenAL_Create(void);

/* Load and play sound */
int  OpenAL_LoadSound(OpenALAudio* audio, const char* filepath);
void OpenAL_PlaySound(OpenALAudio* audio, int sound_id, int loop);

/* 3D positional sound */
void OpenAL_SetListenerPos(OpenALAudio* audio, float x, float y, float z);
void OpenAL_SetListenerVel(OpenALAudio* audio, float x, float y, float z);
void OpenAL_SetSourcePos(OpenALAudio* audio, int source_id, float x, float y, float z);

/* Music */
void OpenAL_PlayMusic(OpenALAudio* audio, const char* filepath);
void OpenAL_SetMusicVolume(OpenALAudio* audio, float volume);

/* Update audio */
void OpenAL_Update(OpenALAudio* audio);

/* Destroy audio system */
void OpenAL_Destroy(OpenALAudio* audio);

#endif /* DMC1_OPENAL_H */
