/*
 * dmc_dump - standalone inspector for Devil May Cry 1 (PS2) asset files.
 *
 * Purpose: establish the real on-disc layout of FSD / EMD / PLD / PWS / TM2 /
 * T32 / XAG containers before any loader code is written, so that loaders are
 * driven by observed bytes instead of guessed headers.
 *
 * Build (MSVC):
 *   cl /nologo /W3 /O2 /std:c17 tools\dmc_dump.c /Fe:build\dmc_dump.exe
 * Build (gcc):
 *   gcc -O2 -Wall -o dmc_dump tools/dmc_dump.c
 *
 * Usage:
 *   dmc_dump FILE                       size + magic + first 0x100 bytes
 *   dmc_dump FILE -hex OFF LEN          hexdump a byte range
 *   dmc_dump FILE -u32 OFF[,OFF...]     read u32 words
 *   dmc_dump FILE -u16 OFF[,OFF...]     read u16 words
 *   dmc_dump FILE -find TEXT            scan whole file for an ASCII tag
 *   dmc_dump FILE -findhex HEXDIGITS    scan whole file for raw bytes
 *   dmc_dump FILE -tm2                  parse TIM2 header + image records
 *   dmc_dump FILE -t32                  parse DMC T32 texture archive
 *   dmc_dump FILE -fsd                  parse FSD/EMD container header
 *   dmc_dump FILE -pld                  parse PLD/PWS model archive
 *   dmc_dump FILE -xag                  parse XAG animation table
 *   dmc_dump FILE -scan                 magic sweep of the whole file
 *
 * Multiple actions may be combined: dmc_dump a.fsd -fsd -hex 0 0x80
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define APPEND_ARRAY_MAX 64
#define SCAN_CHUNK       (4u << 20)
#define SCAN_MAXKIND     512

/* ------------------------------------------------------------------ util */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int32_t rd32s(const uint8_t *p) { return (int32_t)rd32(p); }

/* Printable ASCII of 4 bytes, e.g. id field. */
static void tag4(uint32_t v, char out[8])
{
    int i;
    for (i = 0; i < 4; ++i) {
        unsigned char c = (unsigned char)(v >> (8 * i));
        out[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    out[4] = 0;
}

static uint32_t tag4val(const char *s)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < 4 && s[i]; ++i)
        v |= (uint32_t)(unsigned char)s[i] << (8 * i);
    return v;
}

typedef struct {
    const char *path;
    uint8_t *data;
    size_t size;
} File;

static int file_load(File *f, const char *path)
{
    FILE *fp;
    long n;
    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cannot open %s\n", path);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= 0) {
        fprintf(stderr, "%s: empty or unseekable\n", path);
        fclose(fp);
        return 0;
    }
    f->data = (uint8_t *)malloc((size_t)n);
    if (!f->data) {
        fprintf(stderr, "%s: out of memory (%ld bytes)\n", path, n);
        fclose(fp);
        return 0;
    }
    if (fread(f->data, 1, (size_t)n, fp) != (size_t)n) {
        fprintf(stderr, "%s: short read\n", path);
        free(f->data);
        f->data = NULL;
        fclose(fp);
        return 0;
    }
    fclose(fp);
    f->path = path;
    f->size = (size_t)n;
    return 1;
}

static void file_free(File *f)
{
    free(f->data);
    f->data = NULL;
}

/* Bounds-checked accessor: returns NULL when the range leaves the file. */
static const uint8_t *at(const File *f, size_t off, size_t need)
{
    if (off + need > f->size || off + need < off)
        return NULL;
    return f->data + off;
}

static void hexdump(const File *f, size_t off, size_t len)
{
    size_t i;
    if (off >= f->size) {
        printf("  (offset 0x%llX past end of file, size 0x%llX)\n",
               (unsigned long long)off, (unsigned long long)f->size);
        return;
    }
    if (off + len > f->size)
        len = f->size - off;
    for (i = 0; i < len; i += 16) {
        size_t j, n = len - i < 16 ? len - i : 16;
        printf("  %06zX  ", off + i);
        for (j = 0; j < 16; ++j) {
            if (j < n) printf("%02X ", f->data[off + i + j]);
            else       printf("   ");
            if (j == 7) putchar(' ');
        }
        printf(" |");
        for (j = 0; j < n; ++j) {
            unsigned char c = f->data[off + i + j];
            putchar(c >= 0x20 && c < 0x7F ? c : '.');
        }
        printf("|\n");
    }
}

/* --------------------------------------------------------------- actions */

static void act_hex(const File *f, const char *arg)
{
    unsigned long long off = 0, len = 0x100;
    const char *comma = strchr(arg, ',');
    if (comma) {
        off = strtoull(arg, NULL, 0);
        len = strtoull(comma + 1, NULL, 0);
    } else {
        printf("  -hex expects OFF,LEN - using 0,0x100\n");
    }
    printf("\n== hexdump %zX .. %zX ==\n", (size_t)off, (size_t)(off + len));
    hexdump(f, (size_t)off, (size_t)len);
}

static void act_words(const File *f, const char *arg, int is32)
{
    char buf[512];
    char *tok;
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    printf("\n== %s words ==\n", is32 ? "u32" : "u16");
    for (tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        size_t off = (size_t)strtoull(tok, NULL, 0);
        const uint8_t *p = at(f, off, is32 ? 4 : 2);
        if (!p) {
            printf("  [%06zX] = (outside file)\n", off);
            continue;
        }
        if (is32) {
            uint32_t v = rd32(p);
            char t[8];
            tag4(v, t);
            printf("  [%06zX] = %08X (%-10u, %-11d) as4=\"%s\"\n",
                   off, v, v, (int)rd32s(p), t);
        } else {
            uint32_t v = rd16(p);
            printf("  [%06zX] = %04X (%u)\n", off, v, v);
        }
    }

}

static int hexnib(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void find_pattern(const File *f, const uint8_t *pat, size_t plen, const char *label)
{
    size_t i, hits = 0;
    const size_t limit = 64;
    printf("\n== find %s (%zu bytes) ==\n", label, plen);
    if (plen == 0 || plen > f->size) {
        printf("  nothing to search\n");
        return;
    }
    for (i = 0; i + plen <= f->size; ++i) {
        if (f->data[i] != pat[0]) {
            /* memchr keeps this usable on 300 MB files */
            const uint8_t *n = (const uint8_t *)memchr(f->data + i, pat[0], f->size - i);
            if (!n) break;
            i = (size_t)(n - f->data) - 1;
            continue;
        }
        if (memcmp(f->data + i, pat, plen) == 0) {
            char t[8] = {0};
            if (plen == 4) tag4(rd32(f->data + i), t);
            printf("  %06zX", i);
            if (plen == 4) printf("   \"%s\"", t);
            printf("\n");
            if (++hits == limit) {
                printf("  ... stopped after %zu hits\n", limit);
                return;
            }
        }
    }
    if (!hits)
        printf("  no occurrence\n");
}

static void act_find(const File *f, const char *arg)
{
    find_pattern(f, (const uint8_t *)arg, strlen(arg), arg);
}

static void act_findhex(const File *f, const char *arg)
{
    uint8_t pat[64];
    size_t n = 0;
    while (arg[0] && arg[1] && n < sizeof(pat)) {
        int hi = hexnib(arg[0]), lo = hexnib(arg[1]);
        if (hi < 0 || lo < 0) { arg += 2; continue; }
        pat[n++] = (uint8_t)((hi << 4) | lo);
        arg += 2;
    }
    find_pattern(f, pat, n, "hex pattern");
}

/*
 * Whole-file magic sweep: report every 4-byte-aligned word that spells a
 * printable 4-char tag, with its frequency. This is how an unknown container
 * reveals its record types without any prior knowledge.
 */
static void act_scan(const File *f)
{
    static uint32_t kind[SCAN_MAXKIND];
    static size_t   count[SCAN_MAXKIND];
    static size_t   first[SCAN_MAXKIND];
    size_t nkind = 0, i;
    size_t shown = 0;

    printf("\n== aligned 4-char tag sweep over %zu bytes ==\n", f->size);
    for (i = 0; i + 4 <= f->size; i += 4) {
        uint32_t v = rd32(f->data + i);
        int ok = 1, k;
        size_t j;
        for (k = 0; k < 4; ++k) {
            unsigned char c = (unsigned char)(v >> (8 * k));
            if (c < 0x41 || (c > 0x5A && c < 0x61) || c > 0x7A) { ok = 0; break; }
        }
        if (!ok)
            continue;
        for (j = 0; j < nkind; ++j)
            if (kind[j] == v) break;
        if (j == nkind) {
            if (nkind == SCAN_MAXKIND) continue;
            kind[nkind] = v;
            count[nkind] = 0;
            first[nkind] = i;
            j = nkind++;
        }
        count[j]++;
    }
    /* kind[] is already in order of first occurrence, by construction. */
    for (i = 0; i < nkind; ++i) {
        char t[8];
        if (count[i] < 2 && nkind > 64)
            continue;                 /* hide one-off noise on big files */
        tag4(kind[i], t);
        printf("  %06zX  \"%s\"  x%zu\n", first[i], t, count[i]);
        shown++;
        if (shown == 200) {
            printf("  ... capped at 200 kinds (total %zu)\n", nkind);
            break;
        }
    }

    if (shown == 0)
        printf("  no alphabetic 4-char tags found\n");
}

/* -------------------------------------------------------------- TIM2/T32 */

/*
 * tim2FileTag, as stored at the head of a .TM2 file.
 * Verified against E:\DATA\ETC\CAPCOM.TM2 and friends.
 */
static void act_tm2(const File *f)
{
    const uint8_t *h = at(f, 0, 64);
    uint32_t id, clut_size, img_size;
    uint16_t w, hh;
    uint64_t pixels;
    unsigned bpp;

    printf("\n== TIM2 header (layout per docs/formats.md) ==\n");
    if (!h) {
        printf("  file smaller than a TIM2 header\n");
        return;
    }
    id = rd32(h + 0x00);
    if (id != tag4val("TIM2")) {
        char t[8];
        tag4(id, t);
        printf("  id at 0x00 is \"%s\", not \"TIM2\" - this is not a bare TM2\n", t);
        return;
    }
    clut_size = rd32(h + 0x14);
    img_size  = rd32(h + 0x18);
    w  = rd16(h + 0x24);
    hh = rd16(h + 0x26);
    printf("  0x04 version   = %u\n", rd16(h + 0x04));
    printf("  0x06 flags     = 0x%04X   (NOT the pixel format: same value at every depth)\n",
           rd16(h + 0x06));
    printf("  0x08 tex_ofs   = 0x%08X\n", rd32(h + 0x08));
    printf("  0x0C clut_ofs  = 0x%08X\n", rd32(h + 0x0C));
    printf("  0x10 after_tag = 0x%08X (%u)\n", rd32(h + 0x10), rd32(h + 0x10));
    printf("  0x14 clut_size = 0x%08X (%u entries)\n", clut_size, clut_size / 4u);
    printf("  0x18 img_size  = 0x%08X (%u)\n", img_size, img_size);
    printf("  0x22 fmt_byte  = 0x%02X  0x23 aux = 0x%02X\n", h[0x22], h[0x23]);
    printf("  0x24 width     = %u\n  0x26 height    = %u\n", w, hh);

    pixels = (uint64_t)w * hh;
    if (pixels && (uint64_t)img_size * 8 % pixels == 0) {
        bpp = (unsigned)((uint64_t)img_size * 8 / pixels);
        printf("  derived bpp  = %u  (%s)\n", bpp,
               (bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32) ? "ok" : "UNEXPECTED");
    } else {
        printf("  derived bpp  = cannot divide img_size by %ux%u\n", w, hh);
    }
    printf("  0x40 + img + clut = 0x%llX vs file 0x%zX -> %s\n",
           (unsigned long long)(0x40ull + img_size + clut_size), f->size,
           (0x40ull + img_size + clut_size) == f->size ? "MATCH" : "MISMATCH");
    printf("\n  raster at 0x40:\n");
    hexdump(f, 0x40, 0x20);
    if (clut_size) {
        printf("\n  CLUT at 0x%llX:\n", (unsigned long long)(0x40ull + img_size));
        hexdump(f, (size_t)(0x40ull + img_size), 0x20);
    }
}

/*
 * DMC .T32 archive (see docs/formats.md). Verified on 5 files of different
 * sizes:
 *   0x00 u32 record_count
 *   0x04 u32 (size in 256-byte GS packets) << 16 | 0x40
 *   0x08 u32 0x30   fixed header size
 *   0x0C u32 total_size = raster + CLUT
 *   raster_off = filesize - total_size ; CLUT is the last clut_size bytes.
 * Width and height are NOT in this header - they live in the GS packet stream,
 * which is why this tool only reports the byte geometry.
 */
static void act_t32(const File *f)
{
    const uint8_t *h = at(f, 0, 0x40);
    uint32_t count, w04, total, raster, clut;
    size_t raster_off;

    printf("\n== T32 archive ==\n");
    if (!h) {
        printf("  file smaller than a T32 header\n");
        return;
    }
    if (rd32(h + 0x08) != 0x30) {
        printf("  0x08 = 0x%X, not the 0x30 fixed header - not a T32\n", rd32(h + 0x08));
        return;
    }
    count  = rd32(h + 0x00);
    w04    = rd32(h + 0x04);
    total  = rd32(h + 0x0C);
    raster = (w04 >> 16) << 8;
    clut   = total - raster;
    raster_off = f->size - total;

    printf("  0x00 record_count = %u\n", count);
    printf("  0x04 word         = 0x%08X -> raster = %u GS packets = 0x%X (%u) bytes\n",
           w04, w04 >> 16, raster, raster);
    printf("  0x0C total_size   = 0x%08X (%u)\n", total, total);
    printf("       raster 0x%X + clut 0x%X = 0x%X %s\n", raster, clut, raster + clut,
           (raster + clut == total) ? "= total (ok)" : "!= total (BAD)");
    printf("  raster_off = filesize - total = 0x%zX\n", raster_off);
    printf("  clut_off   = filesize - clut  = 0x%zX\n", f->size - clut);
    printf("  GS packet area 0x%zX .. 0x%zX (%u bytes)\n",
           (size_t)0x40, raster_off, (unsigned)(raster_off - 0x40));

    if (count && count <= 8) {
        printf("\n  record table (%u entries, entry size unknown - dumping 0x10 each):\n", count);
        unsigned i;
        for (i = 0; i < count; ++i)
            hexdump(f, 0x10 + (size_t)i * 0x10, 0x10);
    }
    printf("\n  first raster bytes:\n");
    hexdump(f, raster_off, 0x20);
    if (clut) {
        printf("\n  CLUT entries (4 bytes each, alpha in byte 3):\n");
        hexdump(f, f->size - clut, clut < 0x20 ? clut : 0x20);
    }
}

/* ------------------------------------------------------------- FSD / EMD */

/*
 * FSD and EMD share one container header (see docs/formats.md):
 *   0x00 u32 0
 *   0x04 u32 data_size   - does NOT bound the file, meaning still unknown
 *   0x08 u32 0
 *   0x0C u32 1
 *   0x20 u32 1
 *   0x24 u32 table_size  - BYTES of the table at 0x30, entries are 16 bytes
 *   0x28 u32 counter
 *   0x2C u32 counter
 * The table holds [type, offset, flag, id]; type 0 means a free slot. Verified:
 * the first float geometry starts exactly at 0x30 + table_size.
 */
static void act_fsd(const File *f)
{
    const uint8_t *h = at(f, 0, 0x30);
    uint32_t data_size, table_size, c28, c2C;
    size_t table_off = 0x30, data_off;
    unsigned n, i, shown = 0;

    printf("\n== FSD / EMD container ==\n");
    if (!h) {
        printf("  file smaller than a container header\n");
        return;
    }
    data_size  = rd32(h + 0x04);
    table_size = rd32(h + 0x24);
    c28 = rd32(h + 0x28);
    c2C = rd32(h + 0x2C);
    printf("  0x04 data_size  = 0x%08X (%u)   file 0x%zX  -> data_size does not bound the file\n",
           data_size, data_size, f->size);
    printf("  0x24 table_size = 0x%08X (%u) = %u entries of 16 bytes\n",
           table_size, table_size, table_size / 16u);
    printf("  0x28 counter    = %u\n", c28);
    printf("  0x2C counter    = %u\n", c2C);

    data_off = table_off + table_size;
    printf("  geometry should start at 0x%06x:\n", (unsigned)data_off);
    hexdump(f, data_off, 0x20);

    n = table_size / 16u;
    printf("\n  table entries at 0x%06zX (type offset flag id), non-zero only:\n", table_off);
    if (n == 0) {
        printf("    table is empty (legal: R002.FSD has none)\n");
        return;
    }
    for (i = 0; i < n; ++i) {
        const uint8_t *p = at(f, table_off + (size_t)i * 16, 16);
        uint32_t type, off, flag, id;
        if (!p) break;
        type = rd32(p); off = rd32(p + 4); flag = rd32(p + 8); id = rd32(p + 12);
        if ((type | off | flag | id) == 0)
            continue;                       /* free slot */
        printf("    [%3u] type=%-3u offset=0x%07X flag=%-4u id=0x%X", i, type, off, flag, id);
        if (off && off < f->size) {
            const uint8_t *s = at(f, off, 4);
            if (s) {
                uint32_t bits = rd32(s);
                float fv;
                memcpy(&fv, &bits, sizeof fv);   /* avoids unaligned/mis-typed read */
                printf("  first_float=%-12.4g",
                       (fv == fv && fv > -1e20f && fv < 1e20f) ? fv : 0.0f);
            }
        } else if (off) {
            printf("  (offset past EOF)");
        }
        printf("\n");
        if (++shown == 64) { printf("    ... capped at 64 of %u\n", n); break; }
    }
    if (!shown)
        printf("    all %u slots are zero\n", n);
}

/* ---------------------------------------------------------- PLD / PWS */

/*
 * PLD/PWS archive: a u32 count of sub-model offsets followed by that many
 * absolute offsets. PL00.PLD starts with 0x0B (=11) then 11 ascending offsets.
 */
static void act_pld(const File *f)
{
    const uint8_t *h = at(f, 0, 4);
    uint32_t count, i;

    printf("\n== PLD / PWS archive ==\n");
    if (!h) {
        printf("  empty\n");
        return;
    }
    count = rd32(h + 0x00);
    printf("  0x00 offset_count = %u\n", count);
    if (count > 512) {
        printf("  implausible count, dumping head instead\n");
        hexdump(f, 0, 0x80);
        return;
    }
    printf("  offset table:\n");
    for (i = 0; i < count; ++i) {
        const uint8_t *p = at(f, 4 + (size_t)i * 4, 4);
        const uint8_t *q = (i + 1 < count) ? at(f, 4 + (size_t)(i + 1) * 4, 4) : NULL;
        uint32_t off, next;
        if (!p) break;
        off = rd32(p);
        next = q ? rd32(q) : (uint32_t)f->size;   /* last entry ends at EOF */
        printf("    [%2u] -> 0x%06X", i, off);
        if (off >= next) printf("   (DESCENDING - not a plain offset table)");
        else             printf("   size 0x%X (%u)", next - off, next - off);

        {
            const uint8_t *s = at(f, off, 4);
            if (s) {
                char t[8];
                tag4(rd32(s), t);
                printf("   first4=%08X \"%s\"", rd32(s), t);
            }
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------------- XAG */

static void act_xag(const File *f)
{
    const uint8_t *h = at(f, 0, 0x40);
    uint32_t i;

    printf("\n== XAG animation table ==\n");
    if (!h) {
        printf("  empty\n");
        return;
    }
    printf("  file size 0x%zX (%zu)\n", f->size, f->size);
    printf("  first 16 u32 words:\n");
    for (i = 0; i < 16; ++i) {
        uint32_t v = rd32(h + i * 4);
        char t[8];
        tag4(v, t);
        printf("    [%02u] %08X (%-10u) \"%s\"\n", i, v, v, t);
    }
    /* A constant stride across the head of the file is the record size. */
    {
        uint32_t stride = rd32(h);
        size_t off, same = 0, checked = 0;
        for (off = 0; off + 4 <= f->size && checked < 4096; off += stride ? stride : 4, ++checked) {
            if (rd32(f->data + off) == stride) same++;
            else break;
        }
        printf("  stride guess 0x%X (%u): %zu consecutive records carry it (checked %zu)\n",
               stride, stride, same, checked);
        if (stride) {
            printf("  file_size / stride = %.2f\n", (double)f->size / (double)stride);
            printf("  second record at 0x%X:\n", stride);
            hexdump(f, stride, 0x40);
        }
    }
}

/* --------------------------------------------------------------- gs dump */

/*
 * Пакеты Graphics Synthesizer идут по 16 байт:
 *   байты 0..7   REGISTER: байт 0 - мл. байт адреса REGBITS
 *   байты 8..15  IMADATE / IMMADATE
 * В файлах DMC флаг ADONLY (бит 7 байта 1) не выставлен, поэтому начало
 * пакета ищется по структуре: REGISTER выглядит как ненулевой адрес в
 * байте 0 и нули в байтах 1..7. Это эвристика инспектора, а не часть
 * формата; имена регистров намеренно не печатаются, пока нет точного
 * соответствия - числа надёжнее догадок.
 */
static int reg_like(const uint8_t *p)
{
    return p[0] != 0 && p[1] == 0 && p[2] == 0 && p[3] == 0 &&
           p[4] == 0 && p[5] == 0 && p[6] == 0 && p[7] == 0;
}

static void act_gs(File *f, const char *arg)
{
    size_t off = 0, len = 0, i, end;
    unsigned n = 0;
    char *comma;

    if (!arg || !*arg) { fprintf(stderr, "-gs needs OFF,LEN\n"); return; }
    off = strtoul(arg, &comma, 16);
    if (comma && *comma == ',')
        len = strtoul(comma + 1, NULL, 16);
    if (!len)
        len = 0x400;
    end = off + len;
    if (end > f->size)
        end = f->size;

    printf("\n== GS packets %zX .. %zX ==\n", off, end);
    for (i = off; i + 16 <= end;) {
        const uint8_t *p = at(f, i, 16);
        uint64_t data = 0;
        int b;
        if (!p)
            break;
        if (!reg_like(p)) {
            i += 4;
            continue;
        }
        for (b = 15; b >= 8; --b)
            data = (data << 8) | p[b];
        printf("  %06zX  reg=%02X data=%016llX", i,
               p[0], (unsigned long long)data);
        if (data)
            printf("  lo=%08llX hi=%08llX",
                   (unsigned long long)(data & 0xFFFFFFFFull),
                   (unsigned long long)(data >> 32));
        printf("\n");
        i += 16;
        if (++n >= 4096) {
            printf("  ... truncated at 4096 packets\n");
            break;
        }
    }
    printf("  total %u packets\n", n);
}

/*
 * Честный разбор GIF-потока по спецификации, без эвристик на «похожесть».
 * Тег - 128 бит:
 *   lo: 0-6 NLOOP, 7 EOP, 14-16 IDE, 17 PRE, 18-28 PRIM, 31-34 W,
 *       37-38 FORMAT, 40-43 TRANSFER, 45-50 CMD, 51-63 REGBITS
 * FORMAT: 0=32bit 1=16bit 2=24bit 3=EVENT
 * CMD:    0=IMAGE 1=PACKED 2=REGBITMASK 3=REGBIT
 * PACKED: NLOOP групп, в каждой NREG квадов (регистр 8 байт + данные 8).
 * IMAGE:  NLOOP строк по W*H пикселей выбранной ширины.
 */
static void act_gif(File *f, const char *arg)
{
    size_t off = arg ? strtoul(arg, NULL, 16) : 0;
    int step = 0;
    printf("\n== GIF stream from %zX ==\n", off);
    while (off + 16 <= f->size && step < 64) {
        const uint8_t *p = at(f, off, 16);
        uint64_t lo = 0, hi = 0;
        unsigned nloop, fmt, cmd, nreg, w, i;
        size_t body;
        int b;
        if (!p)
            break;
        for (b = 7; b >= 0; --b) lo = (lo << 8) | p[b];
        for (b = 15; b >= 8; --b) hi = (hi << 8) | p[b];

        nloop = (unsigned)(lo & 0x7Fu);
        fmt   = (unsigned)((lo >> 37) & 3u);
        cmd   = (unsigned)((lo >> 45) & 0x3Fu);
        nreg  = (unsigned)((lo >> 51) & 0x1FFFu);
        w     = (unsigned)((lo >> 31) & 0xFu);

        printf("  %06zX  NLOOP=%u EOP=%u FORMAT=%u CMD=%u NREG=%03X W=%u"
               " TRANSFER=%u hi=%016llX\n",
               off, nloop, (unsigned)((lo >> 7) & 1u), fmt, cmd, nreg, w,
               (unsigned)((lo >> 40) & 0xFu), (unsigned long long)hi);

        if (cmd == 1)
            body = (size_t)nloop * nreg * 16u;
        else if (cmd == 0) {
            unsigned pbytes = (fmt == 1) ? 2u : (fmt == 2) ? 3u : 4u;
            body = (size_t)nloop * (size_t)(w ? w : 1) * 16u;
            (void)pbytes;
        } else
            body = (size_t)nloop * 16u;

        if (nloop == 0 || nloop > 0x20000u) {
            printf("  NLOOP out of range - stopping\n");
            break;
        }
        if (cmd == 1) {
            size_t q = off + 16;
            for (i = 0; i < nloop * nreg && i < 64 && q + 16 <= f->size; ++i, q += 16) {
                const uint8_t *r = at(f, q, 16);
                uint64_t d = 0;
                int k;
                for (k = 15; k >= 8; --k) d = (d << 8) | r[k];
                printf("      reg=%03X data=%016llX\n",
                       (unsigned)(r[0] | ((unsigned)r[1] << 8)),
                       (unsigned long long)d);
            }
        }
        off += 16 + body;
        step++;
    }
}

/* -------------------------------------------------------------- t32 try */

/*
 * Перебор раскладок растра T32 с метрикой. Идея: интерфейс DMC - это
 * крупные однородные области, поэтому правильная раскладка даёт
 * минимальную среднюю разницу между соседними пикселями. Метрика
 * считается по индексам палитры, чтобы не зависеть от порядка каналов.
 * Это способ отсеять заведомо неверные варианты без визуальной проверки.
 */
enum { LAY_LINEAR, LAY_TILE, LAY_TILE512, LAY_TILE1024, LAY_TRANS, LAY_COUNT };

static const char *lay_name[LAY_COUNT] = {
    "linear", "tile pageW=W", "tile pageW=512", "tile pageW=1024", "transposed"
};

static size_t lay_off(int lay, uint32_t W, uint32_t H, uint32_t x, uint32_t y)
{
    switch (lay) {
    case LAY_LINEAR:
        return (size_t)y * W + x;
    case LAY_TILE:
        return (((size_t)((W + 63u) >> 6) * (y >> 5) + (x >> 6)) << 11) |
               (((size_t)(y & 31u) << 6) | (x & 63u));
    case LAY_TILE512:
        return (((size_t)(8u * (y >> 5)) + (x >> 6)) << 11) |
               (((size_t)(y & 31u) << 6) | (x & 63u));
    case LAY_TILE1024:
        return (((size_t)(16u * (y >> 5)) + (x >> 6)) << 11) |
               (((size_t)(y & 31u) << 6) | (x & 63u));
    case LAY_TRANS:
        return (((size_t)((x >> 6) * ((H + 31u) >> 5)) + (y >> 5)) << 11) |
               (((size_t)(x & 63u) << 5) | (y & 31u));
    default:
        return 0;
    }
}

static void act_t32try(File *f)
{
    size_t total, raster_size, raster_off, i;
    uint32_t widths[8];
    int nw = 0, lay, wi;
    static const uint32_t cand[] = { 256, 448, 512, 896, 1024, 2048 };

    if (f->size < 0x40) { printf("too small\n"); return; }
    raster_size = (((uint32_t)f->data[4] | ((uint32_t)f->data[5] << 8) |
                    ((uint32_t)f->data[6] << 16) |
                    ((uint32_t)f->data[7] << 24)) >> 16) << 8;
    total = (uint32_t)f->data[0x0C] | ((uint32_t)f->data[0x0D] << 8) |
            ((uint32_t)f->data[0x0E] << 16) | ((uint32_t)f->data[0x0F] << 24);
    printf("  debug: raster_size=%zu total=%zu fsize=%zu\n",
           raster_size, total, f->size);
    if (!raster_size || total < raster_size || total > f->size) {
        printf("header not T32-like\n");
        return;
    }
    raster_off = f->size - total;

    for (i = 0; i < sizeof(cand) / sizeof(cand[0]); ++i)
        if (raster_size % cand[i] == 0 && raster_size / cand[i] >= 32 &&
            raster_size / cand[i] <= 2048)
            widths[nw++] = cand[i];

    printf("\n== T32 layout search: raster=%zu bytes ==\n", raster_size);
    for (wi = 0; wi < nw; ++wi) {
        uint32_t W = widths[wi], H = (uint32_t)(raster_size / W);
        const uint8_t *r = at(f, raster_off, raster_size);
        for (lay = 0; lay < LAY_COUNT; ++lay) {
            uint64_t acc = 0, pairs = 0, oob = 0;
            uint32_t x, y;
            double score;
            for (y = 0; y < H; ++y) {
                for (x = 0; x < W; ++x) {
                    size_t a = lay_off(lay, W, H, x, y);
                    int av, bv;
                    if (a >= raster_size) { oob++; continue; }
                    av = r[a];
                    if (x + 1 < W) {
                        size_t b = lay_off(lay, W, H, x + 1, y);
                        if (b < raster_size) {
                            bv = r[b];
                            acc += (av > bv) ? (av - bv) : (bv - av);
                            pairs++;
                        }
                    }
                    if (y + 1 < H) {
                        size_t b = lay_off(lay, W, H, x, y + 1);
                        if (b < raster_size) {
                            bv = r[b];
                            acc += (av > bv) ? (av - bv) : (bv - av);
                            pairs++;
                        }
                    }
                }
            }
            score = pairs ? (double)acc / (double)pairs : 999.0;
            printf("  W=%4u H=%4u  %-16s  neighbours=%.2f%s\n",
                   W, H, lay_name[lay], score,
                   oob ? "  (out-of-range reads)" : "");
        }
    }
}

/* ------------------------------------------------------------- t32 map */

/*
 * Карта шума по строкам для T32. Если растр лежит не сплошным блоком,
 * шум идёт периодическими группами строк - по периоду и восстанавливается
 * настоящий модуль строки. Метрика - средний модуль разности R-канала
 * соседних пикселей строки (R выбран, потому что для серых шкал интерфейса
 * он осмыслен, а индексы палитры осмысленной гладкости не дают).
 */
static void act_t32map(File *f)
{
    size_t total, raster_size, raster_off;
    uint32_t W = 256, H, y, x;
    const uint8_t *r;
    uint32_t raster_bytes;

    if (f->size < 0x40)
        return;
    raster_bytes = (((uint32_t)f->data[4] | ((uint32_t)f->data[5] << 8) |
                     ((uint32_t)f->data[6] << 16) |
                     ((uint32_t)f->data[7] << 24)) >> 16) << 8;
    total = (uint32_t)f->data[0x0C] | ((uint32_t)f->data[0x0D] << 8) |
            ((uint32_t)f->data[0x0E] << 16) | ((uint32_t)f->data[0x0F] << 24);
    if (!raster_bytes || total < raster_bytes || total > f->size)
        return;
    raster_size = raster_bytes;
    raster_off = f->size - total;
    r = f->data + raster_off;
    H = raster_size / (W * 4u);

    printf("\n== T32 row noise map (linear, W=%u, 32bpp), H=%u ==\n", W, H);
    for (y = 0; y < H; y += 8) {
        uint64_t acc = 0;
        uint32_t n = 0, g;
        char bar[40];
        double s;
        for (g = 0; g < 8 && y + g < H; ++g) {
            const uint8_t *row = r + (size_t)(y + g) * W * 4u;
            for (x = 0; x + 1 < W; ++x) {
                int a = row[x * 4], b = row[x * 4 + 4];
                acc += (a > b) ? (a - b) : (b - a);
                n++;
            }
        }
        s = n ? (double)acc / n : 0;
        {
            int i, filled = (int)(s / 64.0 * 38.0);
            if (filled > 38) filled = 38;
            for (i = 0; i < filled; ++i) bar[i] = '#';
            bar[i] = 0;
        }
        printf("  rows %3u-%3u  %6.2f  %s\n", y, y + 7, s, bar);
    }
}

/* ------------------------------------------------------------------ main */
static void usage(void)
{
    fprintf(stderr,
        "dmc_dump - DMC1 PS2 asset inspector\n"
        "usage: dmc_dump FILE [-hex OFF,LEN] [-u32 L] [-u16 L] [-find TAG]\n"
        "                     [-findhex HEX] [-tm2] [-t32] [-fsd] [-pld]\n"
        "                     [-xag] [-scan] [-all]\n");
}

int main(int argc, char **argv)
{
    File f;
    int i, actions = 0;
    int do_all = 0;

    if (argc < 2) {
        usage();
        return 2;
    }
    if (!file_load(&f, argv[1]))
        return 1;

    printf("file : %s\n", f.path);
    printf("size : 0x%zX bytes (%zu)\n", f.size, f.size);
    {
        const uint8_t *m = at(&f, 0, 16);
        if (m) {
            char t1[8], t2[8];
            tag4(rd32(m), t1);
            tag4(rd32(m + 4), t2);
            printf("head : \"%s\" \"%s\"  %08X %08X\n", t1, t2, rd32(m), rd32(m + 4));
        }
    }

    for (i = 2; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "-hex")) {
            if (++i >= argc) { fprintf(stderr, "-hex needs an argument\n"); file_free(&f); return 2; }
            act_hex(&f, argv[i]);
        } else if (!strcmp(a, "-u32")) {
            if (++i >= argc) { fprintf(stderr, "-u32 needs an argument\n"); file_free(&f); return 2; }
            act_words(&f, argv[i], 1);
        } else if (!strcmp(a, "-u16")) {
            if (++i >= argc) { fprintf(stderr, "-u16 needs an argument\n"); file_free(&f); return 2; }
            act_words(&f, argv[i], 0);
        } else if (!strcmp(a, "-find")) {
            if (++i >= argc) { fprintf(stderr, "-find needs an argument\n"); file_free(&f); return 2; }
            act_find(&f, argv[i]);
        } else if (!strcmp(a, "-findhex")) {
            if (++i >= argc) { fprintf(stderr, "-findhex needs an argument\n"); file_free(&f); return 2; }
            act_findhex(&f, argv[i]);
        } else if (!strcmp(a, "-tm2")) {
            act_tm2(&f);
        } else if (!strcmp(a, "-t32")) {
            act_t32(&f);
        } else if (!strcmp(a, "-fsd")) {
            act_fsd(&f);
        } else if (!strcmp(a, "-pld")) {
            act_pld(&f);
        } else if (!strcmp(a, "-xag")) {
            act_xag(&f);
        } else if (!strcmp(a, "-t32try")) {
            act_t32try(&f);
        } else if (!strcmp(a, "-t32map")) {
            act_t32map(&f);
        } else if (!strcmp(a, "-gif")) {
            if (++i >= argc) { fprintf(stderr, "-gif needs an argument\n"); file_free(&f); return 2; }
            act_gif(&f, argv[i]);
        } else if (!strcmp(a, "-gs")) {
            if (++i >= argc) { fprintf(stderr, "-gs needs an argument\n"); file_free(&f); return 2; }
            act_gs(&f, argv[i]);
        } else if (!strcmp(a, "-scan")) {
            act_scan(&f);
        } else if (!strcmp(a, "-all")) {
            do_all = 1;
        } else if (!strcmp(a, "-head")) {
            printf("\n== head 0x100 ==\n");
            hexdump(&f, 0, 0x100);
        } else {
            fprintf(stderr, "unknown option %s\n", a);
            usage();
            file_free(&f);
            return 2;
        }
        actions++;
    }

    if (!actions) {
        printf("\n== head 0x100 ==\n");
        hexdump(&f, 0, 0x100);
    }
    if (do_all) {
        act_tm2(&f);
        act_t32(&f);
        act_fsd(&f);
        act_pld(&f);
        act_xag(&f);
    }

    file_free(&f);
    return 0;
}
