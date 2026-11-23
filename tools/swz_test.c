/*
 * swz_test - проверка корректности PSMCT8 swizzle-таблицы.
 *
 * Правильная раскладка страницы обязана быть ПЕРЕСТАНОВКОЙ: каждое смещение
 * 0..8191 встречается ровно один раз. Если есть пропуски/дубликаты - таблица
 * битая, и декодер даст кашу независимо от ширины/высоты.
 *
 * Строит таблицу тем же кодом, что и src/loader/t32.c, и печатает число
 * уникальных значений, первый дубликат и первый пропуск.
 */

#include <stdio.h>
#include <string.h>

static const int PSMT8_PAGE_SWIZZLE[32] = {
     0,  1,  4,  5, 16, 17, 20, 21,
     2,  3,  6,  7, 18, 19, 22, 23,
     8,  9, 12, 13, 24, 25, 28, 29,
    10, 11, 14, 15, 26, 27, 30, 31,
};
static const int PSMT8_COL_SWIZZLE[128] = {
     0,  4, 16, 20, 32, 36, 48, 52,  2,  6, 18, 22, 34, 38, 50, 54,
     8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62,
    33, 37, 49, 53,  1,  5, 17, 21, 35, 39, 51, 55,  3,  7, 19, 23,
    41, 45, 57, 61,  9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31,
    32, 36, 48, 52,  0,  4, 16, 20, 34, 38, 50, 54,  2,  6, 18, 22,
    40, 44, 56, 60,  8, 12, 24, 28, 42, 46, 58, 62, 10, 14, 26, 30,
     1,  5, 17, 21, 33, 37, 49, 53,  3,  7, 19, 23, 35, 39, 51, 55,
     9, 13, 25, 29, 41, 45, 57, 61, 11, 15, 27, 31, 43, 47, 59, 63,
};

static int g_table[128 * 64];

int main(void)
{
    int hist[8192];
    int block, i, unique = 0, firstdup = -1, firstmiss = -1;
    int filled = 0;

    memset(g_table, -1, sizeof(g_table));
    memset(hist, 0, sizeof(hist));

    /*
     * Прямое построение в байтовой шкале. Страница PSMCT8 = 128x64 = 8192
     * байта, разбита на 32 блока 16x16 (256 пикселей). Для пикселя px внутри
     * блока: xoff=px%16, yoff=px/16; байтовое смещение = base + colOff +
     * (px/64)*64, где colOff = blockSwizzle[px%128] (0..63), base -
     * перемешанное смещение страницы.
     */
    for (block = 0; block < 32; ++block) {
        int base = PSMT8_PAGE_SWIZZLE[block] * 256;
        int bxpx = (block % 8) * 16;
        int bypx = (block / 8) * 16;
        for (i = 0; i < 256; ++i) {
            int colOff = PSMT8_COL_SWIZZLE[i % 128];
            int pos = base + colOff + (i / 64) * 64;
            int x = bxpx + (i % 16);
            int y = bypx + (i / 16);
            if (x < 128 && y < 64)
                g_table[y * 128 + x] = pos;
        }
    }

    for (i = 0; i < 128 * 64; ++i)
        if (g_table[i] >= 0) {
            if (++hist[g_table[i]] == 1) ++unique;
            else if (firstdup < 0) firstdup = g_table[i];
            ++filled;
        }
    for (i = 0; i < 8192; ++i)
        if (hist[i] == 0) { firstmiss = i; break; }

    printf("filled cells   = %d (expect 8192)\n", filled);
    printf("unique offsets = %d (expect 8192)\n", unique);
    printf("first duplicate offset = %d\n", firstdup);
    printf("first missing offset   = %d\n", firstmiss);
    printf(filled == 8192 && unique == 8192 ? "BIJECTIVE: OK\n"
                                            : "BROKEN TABLE\n");
    return 0;
}
