/*
 * t32_regs - печать GS-регистров из GIF-пакетов преамбулы T32.
 *
 * T32 содержит не bitmap, а готовые пакеты пересылки в VRAM. Размеры и
 * формат текстуры лежат в регистрах, а не в заголовке файла:
 *
 *   BITBLTBUF (0x50): SBW = bits16..21 (buffer width / 64 пикселей),
 *                     SPSM = bits24..29 (формат пикселя)
 *   TRXPOS    (0x51): DSAX = bits32..42, DSAY = bits48..58 (координаты в VRAM)
 *   TRXREG    (0x52): TW = bits0..11 (ширина), TH = bits32..43 (высота)
 *   TRXDIR    (0x53): dir = bits0..1
 *
 * Пакеты идут парами (qword адреса регистра, qword данных) без GIF-tag, как
 * зафиксировано в docs/formats.md. Инструмент обходит преамбулу и печатает
 * найденные значения для каждого файла, чтобы подобрать верную раскладку.
 *
 * Usage:
 *   t32_regs [ROOT_DIR]      (default assets)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

#define MAX_FILES 512

static char g_files[MAX_FILES][260];
static int  g_nfiles = 0;

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static void add_file(const char *path)
{
    if (g_nfiles < MAX_FILES) {
        strncpy(g_files[g_nfiles], path, 259);
        g_files[g_nfiles][259] = 0;
        g_nfiles++;
    }
}

static void scan_dir(const char *dir)
{
    char pattern[300], sub[300], full[300];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    _snprintf(pattern, sizeof(pattern) - 1, "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        size_t n;
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", dir, fd.cFileName);
            scan_dir(sub);
            continue;
        }
        n = strlen(fd.cFileName);
        if (fd.nFileSizeLow > 0 && n > 4 &&
            !_stricmp(fd.cFileName + n - 4, ".T32")) {
            _snprintf(full, sizeof(full) - 1, "%s\\%s", dir, fd.cFileName);
            add_file(full);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static int is_register(const uint8_t *p, uint8_t addr)
{
    int i;
    if (p[0] != addr)
        return 0;
    for (i = 1; i < 8; ++i)
        if (p[i] != 0)
            return 0;
    return 1;
}

static const char *psm_name(uint32_t psm)
{
    switch (psm) {
    case 0x00: return "PSMCT32";
    case 0x01: return "PSMCT24";
    case 0x02: return "PSMCT16";
    case 0x0A: return "PSMCT16S";
    case 0x13: return "PSMCT8";
    case 0x14: return "PSMCT4";
    case 0x1B: return "PSMCT8H";
    case 0x24: return "PSMCT4HL";
    case 0x2C: return "PSMCT4HH";
    default:   return "?";
    }
}

static void report(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint8_t *buf;
    uint32_t count, raster, table_end, payload, raster_off;
    uint32_t i;
    int found = 0;
    const char *base = strrchr(path, '\\');
    base = base ? base + 1 : path;

    if (!fp)
        return;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0x440) { fclose(fp); return; }
    buf = (uint8_t *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf); fclose(fp); return;
    }
    fclose(fp);

    count     = rd32(buf + 0x00);
    raster    = (rd32(buf + 0x04) >> 16) << 8;
    table_end = rd32(buf + 0x08);
    payload   = rd32(buf + 0x0C);
    raster_off = (uint32_t)(size - payload - 0x400);
    if (raster_off < table_end || raster_off > (uint32_t)size) {
        free(buf);
        return;
    }

    printf("%-16s size=0x%06X raster=0x%06X off=0x%05X count=%u\n",
           base, (uint32_t)size, raster, raster_off, count);

    for (i = table_end; i + 16 <= raster_off; i += 8) {
        if (is_register(buf + i, 0x50)) {
            uint64_t v = rd64(buf + i + 8);
            uint32_t sbp = (uint32_t)(v & 0x3FFF);
            uint32_t sbw = (uint32_t)((v >> 16) & 0x3F);
            uint32_t spsm = (uint32_t)((v >> 24) & 0x3F);
            uint32_t dbw = (uint32_t)((v >> 48) & 0x3F);
            uint32_t dpsm = (uint32_t)((v >> 56) & 0x3F);
            printf("    BITBLTBUF SBP=0x%X SBW=%u(=%upx) SPSM=0x%02X(%s)"
                   " DBW=%u(=%upx) DPSM=0x%02X(%s)\n",
                   sbp, sbw, sbw * 64u, spsm, psm_name(spsm),
                   dbw, dbw * 64u, dpsm, psm_name(dpsm));
            found = 1;
        } else if (is_register(buf + i, 0x51)) {
            uint64_t v = rd64(buf + i + 8);
            uint32_t sasx = (uint32_t)(v & 0x7FF);
            uint32_t sasy = (uint32_t)((v >> 16) & 0x7FF);
            uint32_t dsax = (uint32_t)((v >> 32) & 0x7FF);
            uint32_t dsay = (uint32_t)((v >> 48) & 0x3FF);
            printf("    TRXPOS    SASX=%u SASY=%u DSAX=%u DSAY=%u\n",
                   sasx, sasy, dsax, dsay);
            found = 1;
        } else if (is_register(buf + i, 0x52)) {
            uint64_t v = rd64(buf + i + 8);
            uint32_t tw = (uint32_t)(v & 0xFFF);
            uint32_t th = (uint32_t)((v >> 32) & 0xFFF);
            printf("    TRXREG    TW=%u TH=%u\n", tw, th);
            found = 1;
        } else if (is_register(buf + i, 0x53)) {
            uint64_t v = rd64(buf + i + 8);
            printf("    TRXDIR    dir=%u\n", (uint32_t)(v & 3));
            found = 1;
        }
    }
    if (!found)
        printf("    (no BITBLTBUF/TRXPOS/TRXREG/TRXDIR in preamble)\n");
    free(buf);
}

int main(int argc, char **argv)
{
    const char *root = (argc > 1) ? argv[1] : "assets";
    int i;

    scan_dir(root);
    if (g_nfiles == 0) {
        fprintf(stderr, "no .T32 under %s\n", root);
        return 1;
    }
    for (i = 0; i < g_nfiles; ++i)
        report(g_files[i]);
    return 0;
}
