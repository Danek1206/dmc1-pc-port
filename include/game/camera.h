/*
 * DDMC1 PC Port - Camera System
 * Система камеры в стиле DMC 4/5
 * 
 * Поддерживает:
 *   - 2nd Person (от 3-го лица)
 *   - Следование за игроком
 *   - Авто-корректировку при столкновениях
 *   - Вращение мышью
 *   - Зум (колесо мыши)
 */

#ifndef DMC1_CAMERA_H
#define DMC1_CAMERA_H

typedef struct {
    /* Camera position */
    float pos_x, pos_y, pos_z;
    
    /* Camera target (player position) */
    float target_x, target_y, target_z;
    
    /* Camera rotation */
    float yaw;
    float pitch;
    
    /* Distance from player */
    float distance;
    float min_distance;
    float max_distance;
    
    /* Zoom */
    float zoom;
    
    /* Offset from player */
    float offset_x, offset_y, offset_z;
    
    /* Collision avoidance */
    int collision_enabled;
} CameraData;

/* Create camera */
CameraData* Camera_Create(void);

/* Update camera */
void Camera_Update(CameraData* camera, void* player, void* input, double dt);

/* Setup OpenGL camera */
void Camera_SetupOpenGL(CameraData* camera);

/* Destroy camera */
void Camera_Destroy(CameraData* camera);

#endif /* DMC1_CAMERA_H */
