#!/usr/bin/env python3
"""
dmc_symtab.py - dump and group the symbol table of the DMC1 PS2 executable.

Reads the MIPS r5900 ELF32 little-endian image from the retail disc and writes
docs/engine_map.txt: every global function and every data object, grouped by
the subsystem implied by the original symbol name.

Note on "modules": the ELF carries sections named t_mdview, t_cam, t_flr, ...
but each of them holds only 13 linker markers (_t_cam_text_start and friends)
for an overlay segment at 0x01F00000. All 17841 real symbols sit in st_shndx
of the single "main" section, so grouping by st_shndx is useless. The original
subsystem is only recoverable from the name prefix, which is what module_of()
does.

Usage:
    python tools/dmc_symtab.py E:/SLES_503.58 docs/engine_map.txt
"""

import struct
import sys
import os
import re
from collections import defaultdict

SHT_SYMTAB = 2
STT_FUNC = 2
STT_OBJECT = 1
STB_GLOBAL = 1

SEGMENT = re.compile(r'[A-Z]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+')

# Coarse subsystem a name prefix belongs to. Heuristic on purpose: the binary
# keeps no translation-unit info (see dump() header note), so this only buckets
# prefixes that share an obvious stem. Longest stem wins.
DOMAIN_BY_PREFIX = [
    ('_dec', 'mpeg_decoder'), ('_motion', 'mpeg_decoder'), ('_slice', 'mpeg_decoder'),
    ('_decode', 'mpeg_decoder'), ('_doMC', 'mpeg_decoder'), ('Strip', 'mpeg_decoder'),
    ('Mpeg', 'mpeg_decoder'), ('mpeg', 'mpeg_decoder'), ('Vlc', 'mpeg_decoder'),
    ('sce', 'ps2_library'), ('Sce', 'ps2_library'), ('_sce', 'ps2_library'),
    ('lib', 'ps2_library'), ('_sys', 'ps2_library'),
    ('Pl00', 'player_dante'), ('Pl0', 'player_dante'), ('Pl', 'player_dante'),
    ('Player', 'player_dante'), ('Dante', 'player_dante'), ('Devil', 'player_dante'),
    ('Em0', 'enemy'), ('Em1', 'enemy'), ('Em', 'enemy'), ('Ectrl', 'enemy'),
    ('Enemy', 'enemy'), ('Majin', 'enemy'),
    ('Eff', 'effects'), ('eff', 'effects'), ('Efm', 'effects'), ('seteff', 'effects'),
    ('Cam', 'camera'), ('Camera', 'camera'), ('camera', 'camera'),
    ('Flr', 'floor_collision'), ('flr', 'floor_collision'), ('Floor', 'floor_collision'),
    ('Scr', 'script_scene'), ('scr', 'script_scene'), ('Scene', 'script_scene'),
    ('Eve', 'events'), ('Ev', 'events'), ('ev_', 'events'),
    ('Om', 'room_scripts'),
    ('Snd', 'sound'), ('snd', 'sound'), ('Spu', 'sound'), ('SPU', 'sound'),
    ('Seq', 'sound'), ('seq', 'sound'),
    ('Gauge', 'hud'), ('gauge', 'hud'), ('Hud', 'hud'), ('HUD', 'hud'),
    ('Font', 'hud'), ('font', 'hud'), ('Moji', 'hud'), ('Fnt', 'hud'),
    ('Colbar', 'hud'), ('Select', 'hud'), ('Title', 'hud'), ('Menu', 'hud'),
    ('Item', 'items'), ('item', 'items'),
    ('Weap', 'weapons'), ('weap', 'weapons'), ('Sword', 'weapons'),
    ('MDV', 'model_view'), ('mdv', 'model_view'), ('Mdl', 'model_view'),
    ('mdl', 'model_view'), ('Model', 'model_view'), ('model', 'model_view'),
    ('Mdmatrix', 'model_view'), ('Tmd', 'model_view'), ('tmd', 'model_view'),
    ('Light', 'lighting'), ('light', 'lighting'),
    ('Fsd', 'asset_formats'), ('fsd', 'asset_formats'), ('Pld', 'asset_formats'),
    ('pld', 'asset_formats'), ('Tim', 'asset_formats'), ('tim', 'asset_formats'),
    ('T32', 'asset_formats'), ('t32', 'asset_formats'), ('Xag', 'asset_formats'),
    ('xag', 'asset_formats'), ('Tex', 'asset_formats'), ('tex', 'asset_formats'),
    ('Emd', 'asset_formats'), ('omd', 'asset_formats'), ('Omd', 'asset_formats'),
    ('Gs', 'gs_graphics'), ('GS', 'gs_graphics'), ('gs', 'gs_graphics'),
    ('Vif', 'gs_graphics'), ('vif', 'gs_graphics'), ('Gph', 'gs_graphics'),
    ('Math', 'math'), ('math', 'math'), ('Mth', 'math'), ('Mtx', 'math'),
    ('Mat', 'math'), ('Quat', 'math'), ('Vec', 'math'), ('vec', 'math'),
    ('Dma', 'system_io'), ('dma', 'system_io'), ('Pad', 'system_io'),
    ('pad', 'system_io'), ('Sio', 'system_io'), ('Mem', 'system_io'),
    ('Cd', 'system_io'), ('cd', 'system_io'), ('Heap', 'system_io'),
    ('Sys', 'system_main'), ('sys', 'system_main'), ('Main', 'system_main'),
    ('main', 'system_main'), ('Init', 'system_main'),
    ('Inf', 'info_overlay'), ('inf', 'info_overlay'),
]
DOMAIN_BY_PREFIX.sort(key=lambda kv: -len(kv[0]))
# Appended after the sort: these would otherwise be shadowed by shorter stems.
DOMAIN_BY_PREFIX += [
    ('Md', 'model_view'),
    ('Sdr', 'sound'),
    ('cap', 'math_vu0'),
    ('Motion', 'motion_animation'),
    ('motion', 'motion_animation'),
    ('move', 'motion_animation'),
    ('MOTION', 'motion_animation'),
    ('Xanim', 'motion_animation'),
    ('Movie', 'movie_pss'),
    ('movie', 'movie_pss'),
    ('RFU', 'ps2_library'),          # stubbed IOP calls: RFU082_SetEventFlag...
    ('Gp', 'gs_graphics'),           # Gp_Rengt4N_*, Gp_Sttexture_*: GS 2D prims
    ('e2d', 'effects'),              # 2D effect layer data
    ('rand', 'random'),
    ('R0', 'room_data'), ('R1', 'room_data'), ('R2', 'room_data'),
    ('R3', 'room_data'), ('R4', 'room_data'), ('R5', 'room_data'),
    ('r0', 'room_data'), ('r1', 'room_data'), ('r2', 'room_data'),
    ('r3', 'room_data'), ('r4', 'room_data'), ('r5', 'room_data'),
]



def domain_of(module):
    if module == 'i':
        # iSuspendThread / iReferSemaStatus: EE kernel (libthread) entry points.
        return 'ps2_library'
    for stem, dom in DOMAIN_BY_PREFIX:
        if module == stem or module.startswith(stem):
            return dom
    if module.startswith('t_'):
        return 'overlay_module'
    return '(unclassified)'

# Leading verbs. DMC1 names accessors and helpers as Verb + Subsystem + Rest
# ('SetEffTexture', 'GetPlNowLife', 'CheckOmEvent'), so the subsystem token is
# the segment AFTER the verb; treating 'Set' as a subsystem lumped 400 unrelated
# functions together. Only Capitalised tokens are ever skipped, and the list is
# deliberately closed: 'Main'/'Update'/'Motion' are meaningful stems of their own
# ('MainLoop', 'Motion_move_pl') and must not be eaten.
ACTION_WORDS = set('''
Set Get Is Has Check Put Del Add Calc Draw Load Save Show Hide Select Switch
Change Make Read Write Reset Start Stop Test Wait Clear Copy Find Search Sort
Swap Create Destroy Enable Disable Encode Decode Push Pop Send Recv Flush Seek
Mark Turn Use Fill Clip Flip Kill Alloc Free Count Print Ask Give Take Tell
'''.split())



def module_of(name):
    """Subsystem token of a symbol name.

    'Pl00AtkComboInit' -> 'Pl00'   'CameraUpdate'   -> 'Camera'
    'FlrAtInit'        -> 'Flr'    'MDVViewSet'     -> 'MDV'
    'SetEffTexture'    -> 'Eff'    'GetPlNowLife'   -> 'Pl'
    't_snd_main'       -> 't_snd'  'memclr'         -> 'memclr'
    """
    if name.startswith('t_'):
        parts = name.split('_')
        if len(parts) >= 2:
            return '_'.join(parts[:2])
    segs = SEGMENT.findall(name)
    if not segs:
        return '(odd)'
    i = 0
    # Skip at most two leading verbs ('Set'+'Eff' -> Eff, 'Check'+'Om' -> Om).
    for _ in range(2):
        if i < len(segs) - 1 and segs[i] in ACTION_WORDS:
            i += 1
        else:
            break
    mod = segs[i]
    # Glue splits like 'E' + 'm01' back into 'Em01'.
    if i + 1 < len(segs) and len(mod) <= 2 and segs[i + 1][:1].isdigit():
        mod += segs[i + 1]
    return mod




class Elf32:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        d = self.data
        if d[:4] != b'\x7fELF':
            raise ValueError('not an ELF file')
        if d[4] != 1 or d[5] != 1:
            raise ValueError('expected ELF32 little-endian')
        hdr = struct.unpack_from('<HHIIIIIHHHHHH', d, 16)
        (self.e_type, self.e_machine, self.e_version, self.e_entry,
         self.e_phoff, self.e_shoff, self.e_flags, self.e_ehsize,
         self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum,
         self.e_shstrndx) = hdr
        if self.e_machine != 8:
            raise ValueError('expected MIPS (e_machine=8), got %d' % self.e_machine)

        self.sections = []
        for i in range(self.e_shnum):
            off = self.e_shoff + i * self.e_shentsize
            (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
             sh_link, sh_info, sh_addralign, sh_entsize) = \
                struct.unpack_from('<IIIIIIIIII', d, off)
            self.sections.append(dict(name=sh_name, type=sh_type, flags=sh_flags,
                                      addr=sh_addr, offset=sh_offset, size=sh_size,
                                      link=sh_link, info=sh_info,
                                      addralign=sh_addralign, entsize=sh_entsize))
        shstr = self.sections[self.e_shstrndx]['offset']
        for i, s in enumerate(self.sections):
            s['sname'] = self.cstr(shstr + s['name']) or ('.sec%d' % i)

    def cstr(self, off):
        end = self.data.find(b'\x00', off)
        return '' if end < 0 else self.data[off:end].decode('latin-1')

    def symbols(self):
        st = next((s for s in self.sections if s['type'] == SHT_SYMTAB), None)
        if st is None:
            raise ValueError('no SHT_SYMTAB section')
        strtab = self.sections[st['link']]['offset']
        entsize = st['entsize'] or 16
        out = []
        for i in range(st['size'] // entsize):
            off = st['offset'] + i * entsize
            name_off, value, size, info, other, shndx = \
                struct.unpack_from('<IIIBBH', self.data, off)
            out.append(dict(name=self.cstr(strtab + name_off) if name_off else '',
                            value=value, size=size, bind=info >> 4,
                            type=info & 0xF, other=other, shndx=shndx))
        return out

    def section_name(self, shndx):
        return self.sections[shndx]['sname'] if shndx < len(self.sections) else '#%d' % shndx


def dump(path, dst):
    elf = Elf32(path)
    syms = [s for s in elf.symbols() if s['name']]

    gfuncs = [s for s in syms if s['type'] == STT_FUNC and s['bind'] == STB_GLOBAL]
    lfuncs = [s for s in syms if s['type'] == STT_FUNC and s['bind'] != STB_GLOBAL]
    objs = [s for s in syms if s['type'] == STT_OBJECT]
    others = [s for s in syms if s['type'] not in (STT_FUNC, STT_OBJECT)]

    def bucket(items):
        g = defaultdict(list)
        for s in items:
            g[module_of(s['name'])].append(s)
        return g

    gf, lf, ob = bucket(gfuncs), bucket(lfuncs), bucket(objs)

    # domain -> module -> symbols
    def by_domain(bk):
        d = defaultdict(lambda: defaultdict(list))
        for mod, items in bk.items():
            d[domain_of(mod)][mod] = items
        return d

    gdom, odom = by_domain(gf), by_domain(ob)

    L = []
    w = L.append
    w('# DMC1 (SLES-50358) engine symbol map')
    w('# generated by tools/dmc_symtab.py from %s' % path)
    w('#')
    w('# ELF: MIPS R5900, ET_EXEC, entry 0x%08X, %d sections, %d named symbols' %
      (elf.e_entry, elf.e_shnum, len(syms)))
    w('# Counts: %d global functions, %d static functions, %d objects, %d other' %
      (len(gfuncs), len(lfuncs), len(objs), len(others)))
    w('#')
    w('# IMPORTANT - what the t_* sections really are')
    w('#   The image has sections t_mdview, t_cam, t_light, t_eff_db, t_event,')
    w('#   t_scr_at, t_snd, t_flr, t_inf, t_om_set, t_rck, t_sce_at, t_c_main,')
    w('#   t_movie. Every one of them is an overlay slot at 0x01F00000 holding')
    w('#   only 13 linker markers (_t_xxx_text_start / _bss_end / ...). They do')
    w('#   NOT contain the module code: all real symbols have st_shndx = "main".')
    w('#   The subsystem below is therefore derived from the name prefix, which')
    w('#   is how the original source named things (Pl00* = player part 00,')
    w('#   Em01* = enemy 01, Eff* = effects, Flr* = floor, Cam*/Camera* = camera).')
    w('#   Domains in PART 0 are a heuristic bucketing of those prefixes and are')
    w('#   labelled as such. PART 1 groups are the subsystem token of each name:')
    w('#   a leading verb is skipped, so SetEffTexture lands in Eff and')
    w('#   GetPlNowLife lands in Pl, which is where they belong.')
    w('#')
    w('# How to use this as a behaviour checklist')
    w('#   Each PART 1 group is one subsystem and enumerates the behaviour the')
    w('#   port needs for it:')
    w('#     *Init / *Exit   -> lifecycle you must provide')
    w('#     *Update / *Main -> per-frame logic to reproduce')
    w('#     *Draw / *Render -> presentation')
    w('#     *Get / *Set     -> state the rest of the engine reads; keep it')
    w('#                        layout-compatible with the original')
    w('#   st_value is the original VMA; st_size is the body size in bytes, a')
    w('#   rough measure of how much logic a name hides.')
    w('')

    w('=' * 78)
    w('PART 0 - DOMAIN SUMMARY (heuristic bucketing of name prefixes)')
    w('=' * 78)
    w('%-20s %8s %8s %8s %8s   %s' %
      ('DOMAIN', 'GLOBAL', 'STATIC', 'OBJECTS', 'MODULES', 'LARGEST MODULES'))
    doms = sorted(set(list(gdom) + list(odom)),
                  key=lambda d: (-sum(len(v) for v in gdom.get(d, {}).values()), d))
    for d in doms:
        gmods = gdom.get(d, {})
        omods = odom.get(d, {})
        ng = sum(len(v) for v in gmods.values())
        nl = sum(len(v) for m, v in lf.items() if domain_of(m) == d)
        no = sum(len(v) for v in omods.values())
        top = sorted(gmods.items(), key=lambda kv: -len(kv[1]))[:4]
        w('%-20s %8d %8d %8d %8d   %s' %
          (d, ng, nl, no, len(gmods) + len(omods),
           ' '.join('%s(%d)' % (m, len(v)) for m, v in top)))
    w('')

    w('=' * 78)
    w('PART 1 - GLOBAL FUNCTIONS BY SUBSYSTEM (%d)' % len(gfuncs))
    w('=' * 78)
    w('# Groups are subsystem tokens taken from the names. Tokens carrying a')
    w('# single global function are collected at the end of this part.')
    allmods = sorted(gf, key=lambda m: (-len(gf[m]), m))
    singles = [m for m in allmods if len(gf[m]) == 1]
    for m in allmods:
        items = gf[m]
        if len(items) == 1:
            continue
        w('')
        w('--- %s [%s] : %d functions ---' % (m, domain_of(m), len(items)))
        for s in sorted(items, key=lambda s: s['name']):
            w('  %08X %6d  %s' % (s['value'], s['size'], s['name']))
    w('')
    w('--- single-function subsystem tokens : %d tokens ---' % len(singles))
    for m in singles:
        s = gf[m][0]
        w('  %08X %6d  %-14s %s' % (s['value'], s['size'], m, s['name']))



    w('')
    w('=' * 78)
    w('PART 2 - DATA OBJECTS BY SUBSYSTEM (%d)' % len(objs))
    w('=' * 78)
    w('# st_size tells you how big the original structure was - use it to size')
    w('# the C structs the port has to keep binary compatible.')
    osingles = [m for m in ob if len(ob[m]) == 1]
    for m in sorted(ob, key=lambda m: (-len(ob[m]), m)):
        items = ob[m]
        if len(items) == 1:
            continue
        w('')
        w('--- %s [%s] : %d objects ---' % (m, domain_of(m), len(items)))
        for s in sorted(items, key=lambda s: s['name']):
            w('  %08X %6d  %s' % (s['value'], s['size'], s['name']))
    w('')
    w('--- single-object subsystem tokens : %d tokens ---' % len(osingles))
    for m in sorted(osingles):
        s = ob[m][0]
        w('  %08X %6d  %-14s %s' % (s['value'], s['size'], m, s['name']))


    w('')
    w('=' * 78)
    w('PART 3 - STATIC (file-local) FUNCTIONS (%d)' % len(lfuncs))
    w('=' * 78)
    w('# Not callable from other translation units; shows how a subsystem was')
    w('# factored internally.')
    lsingles = [m for m in lf if len(lf[m]) == 1]
    for m in sorted(lf, key=lambda m: (-len(lf[m]), m)):
        items = lf[m]
        if len(items) == 1:
            continue
        w('')
        w('--- %s [%s] : %d functions ---' % (m, domain_of(m), len(items)))
        for s in sorted(items, key=lambda s: s['name']):
            w('  %08X %6d  %s' % (s['value'], s['size'], s['name']))
    w('')
    w('--- single-function static tokens : %d tokens ---' % len(lsingles))
    for m in sorted(lsingles):
        s = lf[m][0]
        w('  %08X %6d  %-14s %s' % (s['value'], s['size'], m, s['name']))


    w('')
    w('=' * 78)
    w('PART 4 - OVERLAY MARKERS AND OTHER SYMBOLS (%d)' % len(others))
    w('=' * 78)
    for s in sorted(others, key=lambda s: (s['shndx'], s['value'])):
        w('  %08X %6d  t=%-2d b=%d  %-44s [%s]' %
          (s['value'], s['size'], s['type'], s['bind'], s['name'],
           elf.section_name(s['shndx'])))

    d = os.path.dirname(dst)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    print('wrote %s (%d lines)' % (dst, len(L)))
    print('global funcs=%d  static funcs=%d  objects=%d  subsystems=%d' %
          (len(gfuncs), len(lfuncs), len(objs), len(allmods)))


if __name__ == '__main__':
    dump(sys.argv[1] if len(sys.argv) > 1 else 'E:/SLES_503.58',
         sys.argv[2] if len(sys.argv) > 2 else 'docs/engine_map.txt')
