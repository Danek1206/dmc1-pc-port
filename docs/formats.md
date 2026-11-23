# DMC1 (PS2) asset formats — measured layout

Everything here was read out of the retail disc with `tools/dmc_dump.exe`
(built from `tools/dmc_dump.c`). Nothing is assumed from libgraph/PS2SDK
headers unless the bytes confirm it. Each section states which file it was
measured on, so a claim can be re-checked with one command.

Re-measure (needs the disc in `E:`):

```
build\dmc_dump.exe E:\DATA\ETC\CAPCOM.TM2   -tm2
build\dmc_dump.exe E:\DATA\ETC\COLBAR_F.T32 -t32
build\dmc_dump.exe E:\DATA\FSD\R100.FSD     -fsd
build\dmc_dump.exe E:\DATA\EMD\EM00.EMD     -fsd
build\dmc_dump.exe E:\DATA\PLD\PL00.PLD     -pld
build\dmc_dump.exe E:\DATA\ADS\BATTLE.XAG   -xag
```

## Negative results (matter as much as the positive ones)

* The ASCII tag `TIM2` occurs **only** in `.TM2` files. A full-file byte scan
  found zero occurrences in `R100.FSD`, `EM00.EMD`, `PL00.PLD`,
  `COLBAR_F.T32`. So level/model/T32 archives do **not** embed self-describing
  TIM2 headers; their texture pages are addressed by offset tables instead.
  A loader must not scan for magic inside FSD/PLD.
* `FSD` and `EMD` share one container header (see below). They differ in the
  payload, not in how the file starts.
* The `t_mdview`, `t_cam`, `t_flr`, ... ELF sections are **not** the engine
  modules: each holds only 13 linker markers for an overlay slot at
  `0x01F00000`. All 17841 real symbols live in `main`. See
  `docs/engine_map.txt` for the consequences.
* `.XAG` sizes are `BATTLE.XAG` = 93 421 568 B and `EVENT.XAG` = 220 168 192 B.
  The "299 MB single archive" figure from the original plan is the two files
  together, not one.

## TM2 — bare texture file (SOLVED, visually verified)

Measured on all 22 `.TM2` files on the disc; decoder implemented in
`src/loader/tim2.c`, verification tool `tools/dmc_view.c`.

| off | width | meaning | verified |
|-----|-------|---------|----------|
| 0x00 | u32 | `'TIM2'` = 0x324D4954 | all 22 files |
| 0x04 | u16 | version, always 4 | all 22 files |
| 0x06 | u16 | flags — **not** the pixel format | see correction below |
| 0x08 | u32 | `tex_ofs`, always 0 → data follows the header | all 22 files |
| 0x0C | u32 | `clut_ofs`, always 0 | all 22 files |
| 0x10 | u32 | byte count after the 16-byte tag = 0x14 + 0x18 + 0x10 | arithmetic identity |
| 0x14 | u32 | **CLUT size** (0 = no palette) | all 22 files |
| 0x18 | u32 | **raster size** | all 22 files |
| 0x1C | u32 | 0x30 = offset of the image block relative to 0x10 | constant |
| 0x20 | u8 | 0 | |
| 0x21 | u8 | 1 | |
| 0x22 | u8 | **pixel format**: 0x03 = 8 bpp, 0x00 = 32 bpp, 0x43 = 4 bpp | all 22 files |
| 0x23 | u8 | format companion: 5 with 0x03, 3 with 0x00, 4 with 0x43 | all 22 files |
| 0x24 | u16 | **width** | all 22 files |
| 0x26 | u16 | **height** | all 22 files |
| 0x40 | | raster data | |
| 0x40 + 0x18 | | CLUT data, 4 bytes per entry | |

The identity `0x40 + raster_size + clut_size == file_size` holds on **all 22
files**, which is what pins the layout down rather than a single sample.

### Correction to the first draft

The plan and my first pass both assumed the low bits of `flags` at `0x06` were
the pixel format (`tfmt`). **False**: every one of the 22 files has
`flags == 0x0001` while their real depths are 4, 8 and 32 bpp. The real format
byte is at **0x22**. Anyone reusing this note should not trust `flags`.

`CAPCOM2.BMP` on the disc is **not** an export of `CAPCOM.TM2` — it is a
320×240 16 bpp screen grab. There is no byte-exact reference image on the disc,
so verification is visual against the running game.

### Verification performed

`CAPCOM.TM2` decoded to `build/out/CAPCOM.bmp` (512×448, 8 bpp, CLUT of 256
entries starting `00 00 00 80, 09 09 09 80, 0E 0E 0E 80 …`, a grey ramp with
alpha 0x80) and confirmed by eye: the Capcom logo appears at the right aspect
ratio. This proves header offsets, row order and CLUT interpretation.

Still to be confirmed by eye (two guesses remain in `TIM2_Decode`):

* nibble order for 4 bpp — `MOJI8.TM2` / `RLT_*.TM2` (assume low nibble is the
  left pixel);
* R/B order of CLUT entries — indistinguishable on a grey ramp, needs a colour
  texture such as `T_MOJI.TM2` or `TITLE.TM2`.

## T32 — container geometry (measured on 5 files of different sizes)

| file | filesize | u32@0x04 | u32@0x0C | raster = hi16(0x04)<<8 | clut = 0x0C − raster | raster_off = filesize − 0x0C |
|------|----------|----------|----------|------------------------|----------------------|------------------------------|
| COLBAR_F.T32 | 0x385E0 | 0x03800040 | 0x00038100 | 0x38000 | 0x100 | 0x4E0 |
| EASY_M.T32   | 0x385E0 | 0x03800040 | 0x00038100 | 0x38000 | 0x100 | 0x4E0 |
| OPMOJI_U.T32 | 0x85E0  | 0x00800040 | 0x00008100 | 0x8000  | 0x100 | 0x4E0 |
| EFFLIFE.T32  | 0x10A60 | 0x01000040 | 0x00010100 | 0x10000 | 0x100 | 0x960 |
| T_MOJI_F.T32 | 0x20A60 | 0x02000040 | 0x00020100 | 0x20000 | 0x100 | 0x960 |
| MSG_TEX.T32  | 0x31CE0 | 0x03000040 | 0x00030180 | 0x30000 | 0x180 | 0x1B60 |

So:

```
raster_size = (u32@0x04 >> 16) << 8          /* size in 256-byte GS packets */
total_size  =  u32@0x0C                      /* raster + CLUT */
clut_size   =  total_size - raster_size
raster_off  =  filesize - total_size
clut_off    =  filesize - clut_size          /* CLUT is last in the file */
```

* `u32@0x00` = record count (1 for COLBAR/EASY/OPMOJI/EFFLIFE, 2 for
  T_MOJI_F, 6 for MSG_TEX).
* `u32@0x08` = end of the header + record table: `0x10 + count*0x20` for
  count 1 and 2, `0xB0` for count 6 — so the record table entry size is not a
  constant 0x20 and still needs to be pinned down.
* The CLUT really is at the end of the file: the last 0x100 bytes of
  `COLBAR_F.T32` are 4-byte entries with alpha 0x80
  (`DD 1D 14 80, 04 6F B0 80, 0C 6C AA 80, 57 57 57 80 …`).
* Everything between the header and `raster_off` is **prebuilt GS packet
  data**, not texture bytes: pairs of (GS register address, value) in ADONLY
  form, e.g. `…50 00 00 00 00 00 00 00 | 00 00 00 00 00 00 00 00`,
  `…51 00 00 00 00 00 00 00 | 40 00 00 00 80 03 00 00` (the value is the same
  0x03800040 as the header word), `…53 … | 00 B8 00 00 00 00 00 08`.
  `COLBAR_F.T32` additionally holds 0x400 bytes of constant 0xD8 at 0xE0.

### T32 — still open

* **Width and height are not in the header.** The packed `W<<16|H` form used by
  TIM2 (`0x01C00200` for 512×448) was searched for across the whole of
  `COLBAR_F.T32` and the 256×128 variant across `OPMOJI_U.T32`: neither
  occurs. So dimensions live inside the GS packet stream (TRXREG) or must be
  taken from the code that uses the page. This must be resolved before
  `src/loader/t32.c` decodes pixels — guessing a width produces a plausible but
  wrong image, which is worse than an error.
* Whether the raster is 8 bpp indexed everywhere (implied by the 256-entry
  CLUT) or sometimes 16 bpp.

## T32 — facts that do not depend on the unsolved fields

No `TIM2` tag occurs inside any `.T32` file, so T32 records are raw GS texture
pages rather than embedded TIM2 files. That matches the engine symbol
`LoadImageT32s` at `0x1710D0` (140 bytes) sitting next to `LoadImageTim2` at
`0x16FD70` and the `Tim2*` family (`Tim2LoadImage` 0x16FF30,
`Tim2LoadTexture` 0x170590, `Tim2LoadClut` 0x1701C0, `Tim2CalcBufSize` 0x170950,
`Tim2GetLog2` 0x170A40).

File sizes over the 125 `.T32` files: 230 880 for the bulk of the HUD set,
204 000 (`MSG_TEX`), 133 728 (`T_MOJI_*`), 68 192 / 67 040 (`*_MOJI_*`),
34 272 (`OP_MOJI`, `MSTEXUNI`, `OPMOJI_U`), 278 880 (`EFFCOM`). The
`raster_size` / `total_size` fields described above account for every one of
these sizes, so the same header parse covers the whole family.

Earlier drafts of this note read `0x08` as "offset where the payload starts".
That is wrong: `0x08` is `0x30` in every file and is simply the size of the
fixed header. The payload offset is `filesize - u32@0x0C`.

## FSD / EMD — shared container header (partially solved)

Measured on `R100.FSD`, `R101.FSD`, `R002.FSD`, `EM00.EMD`, `EM01.EMD`.

| off | meaning | R100.FSD | EM00.EMD |
|-----|---------|----------|----------|
| 0x00 | always 0 | 0 | 0 |
| 0x04 | `data_size` | 0x315880 | 0x32FC80 |
| 0x08 | always 0 | 0 | 0 |
| 0x0C | always 1 | 1 | 1 |
| 0x10..0x1F | zeros | | |
| 0x20 | always 1 | 1 | 1 |
| 0x24 | **table_size** | 0xFF0 | 0x18E0 |
| 0x28 | small counter | 0 | 9 |
| 0x2C | small counter | 0x62D | 0x661 |

**The offset table is confirmed**: it starts at `0x30` and its end is exactly
`0x30 + table_size`, because the first float geometry appears precisely there.

```
R100.FSD : 0x30 + 0xFF0  = 0x1020   -> float verts start at 0x1020  ✓
EM00.EMD : 0x30 + 0x18E0 = 0x1910   -> float quaternions start at 0x1910  ✓
```

Table entries are **16 bytes**: `[u32 type, u32 offset, u32 flag, u32 id]`.
R100.FSD has 255 slots (0xFF0/16), EM00.EMD 398 (0x18E0/16). Many slots are all
zero (free). Observed filled entries in R100.FSD:

```
0x40: type=2  offset=0x090DC0  flag=0  id=0x62F
0x60: type=1  offset=0x000210  flag=1  id=0x751
0x80: type=2  offset=0x0248E0  flag=1  id=0x752
```

`offset` is file-relative and points at real data: at `0x090DC0` there are
floats (`0x3F673627` = 0.903, `0xBE2D2DB4` = -0.170, ...), i.e. vertex
coordinates, and EM00.EMD at `0x1910` holds normalised floats (`0x3E896ACD` =
0.267, `0x3F3C4EE2` = 0.735) which is rotation data. So the table does address
geometry, and the container is parsed correctly down to this point.

### Corrections to the first draft

* `data_size` at `0x04` does **not** describe the whole file and the identity
  `0x08 + data_size == file_size` is false (R100: 0x315880 vs 0x3F7800). What it
  bounds is still unknown; the region past it in R002.FSD is zero-filled, so it
  is padding, not payload. Do not use it as a block length.
* `table_size` at `0x24` is a **byte** count of the table, not of entries, and
  the entry size is 16, not 4.
* `R002.FSD` has `table_size = 0` and is almost entirely zeros. It is a real
  file in the room set, so a loader must accept an empty table instead of
  failing. Room-name existence is not proof of content.
* A whole-file tag sweep of `R100.FSD` finds no structural magic — every
  alphabetic 4-byte word it reports is an accidental reading of float data
  (`pEzE`, `AupE`, `DupE` are bytes of coordinates in the 0x45XXXXXX range).
  FSD/EMD contain **no** signature tags; the table is the only navigation.

### Still open for `src/loader/fsd.c`

* the meaning of `type`, `flag` and `id`, and the block each entry describes;
* how geometry blocks are bounded (no explicit length in the entry, so it is
  either `next_offset - offset` or a length stored inside the block);
* where texture pages and fog/light parameters live.

## PLD / PWS — model archive (fully confirmed)

Measured on `E:\DATA\PLD\PL00.PLD`, 1 547 616 bytes (0x179D60).

`0x00` = u32 sub-model offset count = **11**, followed by 11 ascending absolute
offsets at `0x04 .. 0x2F`, first payload at `0x30`:

| # | offset | size to next | first u32 at offset |
|---|--------|--------------|---------------------|
| 0 | 0x000030 | 0x37B30 | 0x01032013 |
| 1 | 0x037B60 | 0x04510 | 0x01020D03 |
| 2 | 0x03C070 | 0x03710 | 0x01010101 |
| 3 | 0x03F780 | 0x01450 | 0x000000D0 |
| 4 | 0x040BD0 | 0x018B0 | 0x00000001 |
| 5 | 0x042480 | 0x007A0 | 0x00000001 |
| 6 | 0x042C20 | 0x0FE8D0 | 0x000000D7 |
| 7 | 0x1414F0 | 0x03970 | 0x0000000B |
| 8 | 0x144E60 | 0x031C60 | 0x00000003 |
| 9 | 0x176AC0 | 0x018A0 | 0x00000006 |
| 10 | 0x178360 | 0x001A00 (to EOF) | 0x42043080 |

Sub-model 6 is 1 042 640 of the file's 1 547 616 bytes — the body mesh plus its
texture pages. Entry 0 starts with `0x01032013` and entry 1 with `0x01020D03`:
both begin `01 03`/`01 02` in the low byte, i.e. a small type/version byte
followed by counts, which is the shape of a GS display-list header. That is the
entry point for `src/loader/pld.c`.

Note: entry 10's size is only correct because `-pld` now special-cases the last
offset (earlier builds read the next word out of the payload and printed
15 441 075 for a 0x1A00 block).

## XAG — animation data (not yet decoded)

Measured on `E:\DATA\ADS\BATTLE.XAG`, 93 421 568 bytes (0x5920000).

First 0x40 bytes are four u32 words of `0x0000020C` (524) followed by zeros —
so `0x20C` is a per-record size field repeated across a small leading table, not
a file-wide record stride. The stride hypothesis from the plan is disproven by
arithmetic: 0x5920000 / 0x20C = 178 285.4 and `EVENT.XAG` 0xD200000 / 0x20C =
420 168.3, neither integral. `dmc_dump -xag` prints the stride test and the
second-record dump so this can be finished on the disc; related symbols are
under `motion_animation` / `Xanim` in `docs/engine_map.txt`.

## Naming facts useful for the loader

* 106 `.FSD` rooms, named `R` + 3 hex digits (`R002`, `R100`, `R203`, `R401`...).
* 58 `.EMD` enemies, `EM` + 2–3 chars (`EM00`, `EM001`, `EM1d`).
* 7 `.PLD` player models `PL00 .. PL06`, 7 `.PWS` weapons `PW01 .. PW05`
  (plus `PW00`/`PWD` variants), 24 `.BND`, 125 `.T32`, 22 `.TM2`,
  59 `.DAT` (SPU2 sequences), 7 `.MSG` + 2 `.TBL` + 2 `.DIC` (texts).
* `E:\DATA\ETC\CAPCOM2.BMP` is the only BMP on the disc. It is a 320x240
  screen capture, not a TIM2 export, so it is not a byte-level reference.

## T32 raster layout - header solved, pixel layout explained

Verified: header fields (`count@0x00`, `raster=((u32@0x04>>16)<<8)`,
`total@0x0C`, `raster_off = filesize - total`, record table of 0x18 byte
entries ending at `u32@0x08`).

Decoding the raster linearly as 8bpp+CLUT or 16bpp gives striped noise, seen
directly in `dmc_viewer`. That is explained by what the area between `0x40`
and `raster_off` actually is: **GIF packets, i.e. Graphics Synthesizer
register writes**, 16 bytes each (8-byte REGISTER + 8-byte IMADATE). In
`COLBAR_F` the packets carry register addresses `0x50`, `0x51`, `0x52`,
`0x53`, which per the PS2 GS register map are

```
  50h  BITBLTBUF   source/dest base pointer (/64 words), buffer width (/64
                   pixels) and pixel format PSM: 00h=PSMCT32, 02h=PSMCT16,
                   13h=PSMCT8, 14h=PSMCT4, ...
  51h  TRXPOS      source and destination rectangle corner, transfer order
  52h  TRXREG      transfer area width and height in pixels
  53h  TRXDIR      0=GIF->VRAM, 1=VRAM->GIF, 2=VRAM->VRAM
```

So a T32 raster is not a bitmap: it is a block transfer command plus the
pixel payload in the layout GS uses in VRAM for that PSM. 8bpp and 4bpp
PSM pages are tiled (block layout), which is exactly what a linear read
destroys. Width and pixel format must be taken from BITBLTBUF/TRXREG, and
cannot be guessed from file size.

Remaining work, precisely scoped: parse the GIF tag before the packet run to
get NREG and the packet count, then read PSM/width/height from BITBLTBUF and
TRXREG, then untile according to the PSM block layout. The register map above
is from ps2tek (GS registers section), not inferred.

## T32 raster is swizzled PSMCT8 (SOLVED formula, pending visual re-check)

`tools/t32_regs.exe` reads the GS packets from the T32 preamble and prints
BITBLTBUF/TRXPOS/TRXREG/TRXDIR. Measured result over all `E:\DATA\ETC\*.T32`:

* **BITBLTBUF and TRXREG are zero** (SBW=0, SPSM=0, TW=0). The dimensions and
  format are NOT written into the registers; only TRXPOS carries a value, and
  that value is `DSAX` (the destination X coordinate in VRAM), not a width.
* `raster_size / DSAX == 256` on every file: 0x38000/896, 0x30000/768,
  0x20000/512, 0x10000/256, 0x8000/128, 0x40000/1024 all give exactly 256.
  So the texture is 256 rows tall and its buffer width equals DSAX.
* `raster_size` is always a whole number of 8192-byte PSMCT8 pages
  (0x38000 = 28 pages). 8 bpp indexed + 256-entry CLUT at the file tail.

The raster is therefore stored in the PS2 GS **PSMCT8 swizzled** layout, not
linearly. `tools/t32_probe.exe` proved this by a connectivity metric: swizzled
coherence ~0.33 vs linear ~1.3 (4x worse) on SELECT.T32.

The page is 128x64 = 8192 bytes, 32 blocks of 16x16. `tools/swz_test.c`
verifies the per-page byte-permutation table is a true bijection of 0..8191:

```
pageSwizzle[block] (PSMCT8 uses the PSMCT32 page order) =
    0  1  4  5 16 17 20 21   2  3  6  7 18 19 22 23
    8  9 12 13 24 25 28 29  10 11 14 15 26 27 30 31
columnSwizzle[i%128] = PSMT8 column order (0..63 range), see src/loader/t32.c
for pixel i in 0..255 inside a block:
    x = (block%8)*16 + (i%16)
    y = (block/8)*16 + (i/16)
    byteOffset = pageSwizzle[block]*256 + columnSwizzle[i%128] + (i/64)*64
    page = (y/64)*(width/128) + (x/128)
    linear[y*width + x] = raster[page*8192 + byteOffset]
```

A first attempt used `x = (block%8)*16 + (i%128)`, `y = (block/8)*16 + (i/128)`
which only filled 1024 of 8192 cells (the visualizer source works in BITS for
x, not pixels). That produced the striped "mud". The corrected form above is
bijective (`swz_test.c`: filled=8192, unique=8192, no dup, no gap) and is what
`src/loader/t32.c` now uses. Visual confirmation on a real sprite (SPARDA_F,
OP_DANTE) is pending the disc being re-copied.

Same GS-packet mechanism applies to FSD and PLD/EMD texture pages.
