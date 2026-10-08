#!/usr/bin/env python3
"""Reads TH16 replay files and compares two of them (NOTES.md, Testing,
"Replay recording tests").

    rpy_compare.py show FILE
    rpy_compare.py compare SOURCE RECORDED

show prints the file's header, info and each stage's snapshot. compare
checks a replay the port recorded (RECORDED, with th16 --record-from
SOURCE) against the replay it took its input from: the file header, the
info (except the name, timestamp, slow rate and the recording settings),
every stage's snapshot (the replay RNG seed, g_Globals, the player's
position and focus, how many spell cards ended) and the input of every
frame both files have (the original's recording keeps adding frames to a
stage while the next one loads, so the stages' frame counts may differ;
playback does not use those frames). The exit code is 0 if nothing
differs, 1 otherwise.

Expected differences, listed as notes: the spell cards' capture times
(Spellcard::time_code: real time, from get_runtime), the settings, and
what the previous game in the original's process left in g_Globals: the
first stage's snapshot is taken before the stage starts, and a new game
does not reset the per-chapter values (the stage does), so they are the
previous game's there (zero in a fresh process, like the port's); and
graze_in_chapter and enemies_*_in_chapter are never reset at all, so they
differ by the same amount in every later stage.

The data after the header is LZSS compressed (Lzss.cpp) and encrypted
twice (Crypt.cpp, as ReplayManager::save does it); this reads it the way
ReplayManager::read_replay_file does.
"""
import argparse
import struct
import sys

RPY_MAGIC = b't16r'
HEADER_SIZE = 0x24
INFO_SIZE = 0xa0
GAMESTATE_SIZE = 0x294
GLOBALS_OFFSET = 0x14
GLOBALS_SIZE = 0x228
FRAME_SIZE = 6

# The first 0x228 bytes of Globals (the snapshot) as 32-bit words
# (src/Globals.h).
GLOBALS_NAMES = dict(enumerate([
    'stage_num', 'weird_stage_num', 'chapter', 'time_in_stage', 'time_in_chapter', 'character', 'subshot',
    'subseason', 'score', 'difficulty', 'continues_used', 'rank', 'graze', 'graze_in_chapter', 'spell_id',
    'miss_count', 'unk_40', 'num_point_items_collected', 'piv', 'initial_piv', 'max_piv', 'power', 'max_power',
    'power_per_level', 'unk_60', 'lives', 'life_fragments', 'next_score_extend_index', 'bombs', 'bomb_fragments',
    'season_power', 'max_season_power',
] + ['season_level_deltas[%d]' % i for i in range(10)] + ['season_level_thresholds[%d]' % i for i in range(8)] + [
    'unk_c8', 'unk_cc', 'full_value_item_score', 'unk_d4', 'full_value_item_count', 'unk_dc', 'last_collect_pos.x',
    'last_collect_pos.y', 'last_collect_pos.z', 'item_spawn_count', 'enemies_spawned_in_chapter',
    'enemies_destroyed_in_chapter',
]))
GLOBALS_NAMES = {i * 4: name for i, name in GLOBALS_NAMES.items()}
GLOBALS_NAMES.update({off: 'music_filename+%#x' % (off - 0xf8) for off in range(0xf8, 0x1f8, 4)})
GLOBALS_NAMES.update({off: 'unk_%x' % off for off in range(0x1f8, GLOBALS_SIZE, 4)})
# Left by the previous game in the first stage's snapshot.
LEFTOVER = {name for name in GLOBALS_NAMES.values()
            if name in ('chapter', 'time_in_stage', 'time_in_chapter', 'graze_in_chapter', 'unk_dc',
                        'enemies_spawned_in_chapter', 'enemies_destroyed_in_chapter')
            or name.startswith(('last_collect_pos', 'music_filename', 'unk_1f', 'unk_2'))}
# Counters that nothing resets (they only ever add up, across games), so
# the previous game's count stays in them for the whole game: in later
# stages they may differ from the source only by the first stage's
# difference.
CARRIED = {'graze_in_chapter', 'enemies_spawned_in_chapter', 'enemies_destroyed_in_chapter'}
# Spell capture times not yet written: ReplayManager::initialize fills the
# first stage's with i * 0xdeaddead, later stages start from zero.
PLACEHOLDERS = {(i * 0xdeaddead) & 0xffffffff for i in range(20)} | {0}


def spell_time(code):
    """Spellcard::decode_time_code, as seconds; None for a bad code."""
    code &= 0xffffffff
    if code in PLACEHOLDERS or code >= 0x80000000:
        return None
    hundredths = (code % 100 + 67) % 100
    seconds = (code // 100 % 1000 + 934) % 1000
    if code // 100000 - 22 != seconds + hundredths:
        return None
    return round(seconds + hundredths / 100, 2)


def decrypt(data, key, step, block, limit):
    """zun_decrypt."""
    size = len(data)
    out = bytearray(data)
    tail = size % block
    if tail >= block // 4:
        tail = 0
    remaining = (size & ~1) - tail
    src = bytes(data[:min(limit, size)])
    o = 0
    i = 0
    while remaining > 0 and limit > 0:
        if remaining < block:
            block = remaining
        end = o + block
        o = end
        p = end - 1
        for _ in range((block + 1) // 2):
            out[p] = src[i] ^ key
            p -= 2
            key = (key + step) & 0xff
            i += 1
        p = end - 2
        for _ in range(block // 2):
            out[p] = src[i] ^ key
            p -= 2
            key = (key + step) & 0xff
            i += 1
        remaining -= block
        limit -= block
    return bytes(out)


def lzss_decompress(data, out_size):
    """lzss_decompress: 13-bit window, 4-bit lengths, MSB first."""
    window = bytearray(1 << 13)
    out = bytearray()
    bits = 0
    nbits = 0
    pos = 0

    def read(count):
        nonlocal bits, nbits, pos
        value = 0
        for _ in range(count):
            if nbits == 0:
                bits = data[pos] if pos < len(data) else 0
                pos += 1
                nbits = 8
            nbits -= 1
            value = (value << 1) | ((bits >> nbits) & 1)
        return value

    current = 1
    while len(out) < out_size:
        if read(1):
            c = read(8)
            window[current] = c
            out.append(c)
            current = (current + 1) & 0x1fff
        else:
            match = read(13)
            if match == 0:
                break
            length = read(4) + 2
            for i in range(length + 1):
                c = window[(match + i) & 0x1fff]
                window[current] = c
                out.append(c)
                current = (current + 1) & 0x1fff
    return bytes(out)


class Replay:
    def __init__(self, path):
        raw = open(path, 'rb').read()
        if raw[:4] != RPY_MAGIC:
            raise SystemExit(f'{path}: not a TH16 replay')
        self.header = raw[:HEADER_SIZE]
        self.version, = struct.unpack_from('<H', raw, 4)
        self.user_offset, self.unk_10 = struct.unpack_from('<II', raw, 0xc)
        self.compressed_size, self.size = struct.unpack_from('<II', raw, 0x1c)
        data = raw[HEADER_SIZE:HEADER_SIZE + self.compressed_size]
        data = decrypt(data, 0x5c, 0xe1, 0x400, self.compressed_size)
        data = decrypt(data, 0x7d, 0x3a, 0x100, self.compressed_size)
        self.data = lzss_decompress(data, self.size)
        self.user = raw[self.user_offset:]
        info = self.data[:INFO_SIZE]
        self.info_raw = info
        self.name = info[:0xa].split(b'\0')[0].decode('cp932', 'replace')
        self.timestamp, self.score = struct.unpack_from('<qI', info, 0xc)
        self.config = info[0x18:0x7c]
        self.slowdown, = struct.unpack_from('<f', info, 0x7c)
        (self.num_stages, self.character, self.subshot, self.difficulty, self.stage,
         self.continues_used, self.spell_id, self.subseason) = struct.unpack_from('<8i', info, 0x80)
        self.flags_a, = struct.unpack_from('<H', info, 0xa)
        self.stages = []
        offset = INFO_SIZE
        for _ in range(self.num_stages if self.num_stages < 8 else 6):
            gs = self.data[offset:offset + GAMESTATE_SIZE]
            stage, rng, num_frames, data_size = struct.unpack_from('<hhii', gs, 0)
            inputs = self.data[offset + GAMESTATE_SIZE:offset + GAMESTATE_SIZE + num_frames * FRAME_SIZE]
            fps = self.data[offset + GAMESTATE_SIZE + num_frames * FRAME_SIZE:
                            offset + GAMESTATE_SIZE + data_size]
            self.stages.append({
                'stage': stage, 'rng': rng & 0xffff, 'num_frames': num_frames, 'data_size': data_size,
                'player': struct.unpack_from('<2i', gs, 0xc),
                'globals': gs[GLOBALS_OFFSET:GLOBALS_OFFSET + GLOBALS_SIZE],
                'focused': struct.unpack_from('<i', gs, 0x23c)[0],
                'spell_time_codes': struct.unpack_from('<20i', gs, 0x240),
                'flags_290': struct.unpack_from('<I', gs, 0x290)[0],
                'inputs': [struct.unpack_from('<3H', inputs, i) for i in range(0, len(inputs), FRAME_SIZE)],
                'fps': fps,
            })
            offset += GAMESTATE_SIZE + data_size


def show(args):
    r = Replay(args.file)
    print(f'version {r.version}, unk_10 {r.unk_10:#x}, {r.compressed_size} bytes compressed, {r.size} decompressed')
    print(f'name {r.name!r}, character {r.character}, subshot {r.subshot}, subseason {r.subseason}, '
          f'difficulty {r.difficulty}, stage {r.stage}, score {r.score * 10}, continues {r.continues_used}, '
          f'spell {r.spell_id}, flags_a {r.flags_a:#x}, slowdown {r.slowdown:.2f}')
    for s in r.stages:
        g = s['globals']
        score, = struct.unpack_from('<I', g, 0x20)
        end = s['inputs'][-1] if s['inputs'] else None
        print(f"  stage {s['stage']}: {s['num_frames']} frames ({len(s['fps'])} fps samples), rng {s['rng']}, "
              f"score {score * 10}, player {s['player']}, focused {s['focused']}, flags_290 {s['flags_290']:#x}, "
              f"last input {end}")


def compare(args):
    a = Replay(args.source)
    b = Replay(args.recorded)
    problems = []
    notes = []

    def differ(what, x, y, counts=True):
        (problems if counts else notes).append(f'{what}: source {x}, recorded {y}')

    for name in ('version', 'unk_10', 'num_stages', 'character', 'subshot', 'subseason', 'difficulty',
                 'stage', 'continues_used', 'spell_id', 'flags_a', 'score'):
        if getattr(a, name) != getattr(b, name):
            differ(f'info.{name}', getattr(a, name), getattr(b, name))
    if a.config != b.config:
        differ('info.config', a.config.hex(), b.config.hex(), False)
    if len(a.stages) != len(b.stages):
        differ('stages', [s['stage'] for s in a.stages], [s['stage'] for s in b.stages])
    # Each CARRIED field's difference in the first stage's snapshot.
    carried = {}
    for sa, sb in zip(a.stages, b.stages):
        st = sa['stage']
        for name in ('stage', 'rng', 'player', 'focused', 'flags_290'):
            if sa[name] != sb[name]:
                differ(f'stage {st} {name}', sa[name], sb[name])
        # Real time: only how many cards ended (with a valid code) has to
        # match.
        ta = [spell_time(c) for c in sa['spell_time_codes']]
        tb = [spell_time(c) for c in sb['spell_time_codes']]
        na = sum(t is not None for t in ta)
        nb = sum(t is not None for t in tb)
        if na != nb:
            differ(f'stage {st} spell cards ended', na, nb)
        elif ta != tb:
            differ(f'stage {st} spell capture times (s)', [t for t in ta if t is not None],
                   [t for t in tb if t is not None], False)
        ga, gb = sa['globals'], sb['globals']
        first_stage = sa['flags_290'] & 1
        for off in range(0, GLOBALS_SIZE, 4):
            x, = struct.unpack_from('<i', ga, off)
            y, = struct.unpack_from('<i', gb, off)
            if x == y:
                continue
            name = GLOBALS_NAMES[off]
            if first_stage:
                carried[name] = x - y
                differ(f'stage {st} g_Globals.{name}', x, y, name not in LEFTOVER)
            elif name in CARRIED and x - y == carried.get(name):
                differ(f'stage {st} g_Globals.{name} (the first stage\'s difference, carried)', x, y, False)
            else:
                differ(f'stage {st} g_Globals.{name}', x, y)
        if sa['num_frames'] != sb['num_frames']:
            differ(f'stage {st} frames', sa['num_frames'], sb['num_frames'], False)
        n = min(len(sa['inputs']), len(sb['inputs']))
        bad = [i for i in range(n) if sa['inputs'][i] != sb['inputs'][i]]
        if bad:
            i = bad[0]
            differ(f'stage {st} input ({len(bad)} frames differ, first at frame {i})', sa['inputs'][i],
                   sb['inputs'][i])
        end_a = sa['inputs'][-1] if sa['inputs'] else None
        end_b = sb['inputs'][-1] if sb['inputs'] else None
        marker = (0xffff, 0xffff, 0xffff)
        if (end_a == marker) != (end_b == marker):
            differ(f'stage {st} end marker', end_a == marker, end_b == marker)
    for line in notes:
        print('  note   ', line)
    for line in problems:
        print('  DIFFERS', line)
    print(f'{len(problems)} differences, {len(notes)} expected ones')
    return 1 if problems else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('show')
    p.add_argument('file')
    p = sub.add_parser('compare')
    p.add_argument('source')
    p.add_argument('recorded')
    args = parser.parse_args()
    if args.command == 'show':
        show(args)
        return 0
    return compare(args)


if __name__ == '__main__':
    sys.exit(main())
