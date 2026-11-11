/*
 * DMC1 PC Port - Asset Loader System
 * Загрузка PS2 ассетов: модели (PLD), текстуры (TIM2), уровни (FSD)
 */

#ifndef DMC1_LOADER_H
#define DMC1_LOADER_H

#include <stdint.h>

/* ============================================
 * TIM2 — PS2 Texture Format
 * ============================================ */

/* TIM2 header structure */
typedef struct {
    uint32_t magic;       /* 0x324D4954 = "TIM2" */
    uint16_t version;     /* Version (usually 4) */
    uint16_t flags;       /* Compression flags */
    uint32_t width;       /* Texture width */
    uint32_t height;      /* Texture height */
    uint32_t palette;     /* Palette offset (for 16-color palettized) */
    uint32_t image;       /* Image data offset */
} TIM2Header;

/* TIM2 texture data */
typedef struct {
    uint32_t id;          /* OpenGL texture ID */
    uint32_t width;
    uint32_t height;
    uint32_t bpp;         /* Bits per pixel */
    uint8_t* data;        /* Raw pixel data */
    uint32_t data_size;
} TIM2Texture;

/* Load TIM2 texture from file */
TIM2Texture* TIM2_Load(const char* filepath);

/* Convert TIM2 to OpenGL texture */
void TIM2_ToOpenGL(TIM2Texture* tex);

/* Free TIM2 texture */
void TIM2_Free(TIM2Texture* tex);

/* ============================================
 * PLD — PS2 Model Format
 * ============================================ */

/* PLD vertex structure */
typedef struct {
    float x, y, z;        /* Position */
    float nx, ny, nz;     /* Normal */
    float u, v;           /* Texture coordinate */
    uint8_t r, g, b, a;   /* Color */
} PLDVertex;

/* PLD polygon type */
typedef enum {
    PLD_TRIANGULAR = 0,   /* 3 vertices */
    PLD_RECTANGULAR = 1   /* 4 vertices */
} PLDPolygonType;

/* PLD polygon (face) */
typedef struct {
    uint16_t type;        /* PLDPolygonType */
    uint16_t tex_page;    /* Texture page ID */
    uint16_t flags;       /* Drawing flags */
    uint16_t vertex[4];   /* Vertex indices (max 4) */
} PLDPolygon;

/* PLD sub-model (mesh) */
typedef struct {
    PLDVertex* vertices;
    uint32_t vertex_count;
    PLDPolygon* polygons;
    uint32_t polygon_count;
} PLDSubModel;

/* PLD model data */
typedef struct {
    uint32_t sub_model_count;
    PLDSubModel* sub_models;
} PLDModel;

/* Load PLD model from file */
PLDModel* PLD_Load(const char* filepath);

/* Free PLD model */
void PLD_Free(PLDModel* model);

/* ============================================
 * FSD — PS2 Level/Scene Format
 * ============================================ */

/* FSD node in BSP tree */
typedef struct {
    float plane[4];       /* Plane equation: ax + by + cz + d = 0 */
    int32_t front_child;  /* Index of front child (-1 if leaf) */
    int32_t back_child;   /* Index of back child (-1 if leaf) */
    uint32_t poly_start;  /* Polygon data offset */
    uint32_t poly_count;  /* Number of polygons */
} FSDNode;

/* FSD level data */
typedef struct {
    char name[64];        /* Level name */
    FSDNode* nodes;
    uint32_t node_count;
    PLDVertex* vertices;
    uint32_t vertex_count;
    PLDPolygon* polygons;
    uint32_t polygon_count;
    float fog_r, fog_g, fog_b;
    float fog_density;
} FSDLevel;

/* Load FSD level from file */
FSDLevel* FSD_Load(const char* filepath);

/* Free FSD level */
void FSD_Free(FSDLevel* level);

/* ============================================
 * Asset Manager
 * ============================================ */

/* Asset types */
typedef enum {
    ASSET_TEXTURE,
    ASSET_MODEL,
    ASSET_LEVEL
} AssetType;

/* Asset entry */
typedef struct {
    AssetType type;
    char filepath[256];
    void* data;           /* TIM2Texture*, PLDModel*, FSDLevel* */
    uint32_t ref_count;
} AssetEntry;

/* Initialize asset manager */
void AssetManager_Init(void);

/* Load asset by path */
void* AssetManager_Load(const char* path, AssetType type);

/* Unload asset by path */
void AssetManager_Unload(const char* path);

/* Destroy asset manager */
void AssetManager_Destroy(void);

#endif /* DMC1_LOADER_H */
