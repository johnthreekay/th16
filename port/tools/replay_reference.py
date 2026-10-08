#!/usr/bin/env python3
"""A frame-by-frame trace of a replay played by the original th16.exe, for
finding where the port desyncs (NOTES.md, Testing, "Replay sync tests").

    replay_reference.py record --game-dir DIR --replay FILE --out DUMP
                               [--items FIRST-LAST] [--stop-after FRAME]
                               [--menu-downs N] [--display N]
    replay_reference.py compare REFERENCE_DUMP PORT_DUMP

record runs th16.exe under Wine (in $WINEPREFIX, which must be set: use a
scratch prefix, the script writes the game's save folder there) inside
Xvfb, opens the replay through the menus with xdotool, and reads the game's
memory through /proc/PID/mem every millisecond. For each replay frame it
keeps the last sample (taken after that frame's tick) and writes the same
lines as the port's TH16_REPLAY_TEST_DUMP: stage, frame, g_Globals (the
0x228 bytes replays store), the replay RNG and the player's position, all
as hex; with --items, also every active item of those frames ("I" lines,
as TH16_REPLAY_TEST_ITEMS writes them).

The replay is copied into the save folder as th16_01.rpy, the first entry
of the replay menu. The menu is reached with --menu-downs presses of Down
from Game Start (3 on a save folder without a score file, where Extra
Start is locked and skipped). The configuration is written for a 640x480
window without the startup dialog.

compare prints, for each field, the first frame where the two dumps
differ. time_in_stage and time_in_chapter are left out: the port's dump is
taken after the game thread's tick (priority 0xf) of the next frame, so they
are one ahead there.

Several runs at once need a Wine prefix and an Xvfb display number
(--display) each: xvfb-run's own choice of a free display races when two
start together, and keys then go to the other game.

Needs Wine, Xvfb (xvfb-run), xdotool and, with ptrace_scope 1, nothing
else: Wine's launcher exits after starting the game, and the script makes
itself a child subreaper so that the game process becomes its child, which
reading its memory requires.
"""
import argparse
import ctypes
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

G_GLOBALS = 0x4a5790
G_SAFE_RNG = 0x4a6d88
G_PLAYER = 0x4a6ef8
PLAYER_POS_SUBPIXEL = 0x61c
G_REPLAY_MANAGER = 0x4a6f08
REPLAY_STAGE_NUM = 0x214
REPLAY_FRAME_CURRENT = 0xd8
REPLAY_STAGE_SIZE = 0x28
G_ITEM_MANAGER = 0x4a6ddc
ITEMS = 0x14
ITEM_SIZE = 0xc78
ITEM_COUNT = 0x1258
GLOBALS_SIZE = 0x228
# Counts frames in the title menu without input; the demo starts at 1800.
G_TITLE_IDLE_FRAMES = 0x4a5bf0


def write_save_folder(prefix, replay):
    users = os.path.join(prefix, 'drive_c', 'users')
    user = next(u for u in os.listdir(users) if u != 'Public')
    save = os.path.join(users, user, 'AppData', 'Roaming', 'ShanghaiAlice', 'th16')
    os.makedirs(os.path.join(save, 'replay'), exist_ok=True)
    shutil.copyfile(replay, os.path.join(save, 'replay', 'th16_01.rpy'))
    # The game's defaults (ConfigData::set_defaults), in a 640x480 window
    # (window_size 3) and without the startup dialog.
    cfg = bytearray(0x64)
    struct.pack_into('<I', cfg, 0, 0x160002)
    # g_pad_mapping
    struct.pack_into('<10h', cfg, 4, 0, 1, 2, 5, -1, -1, -1, -1, -1, 3)
    struct.pack_into('<hh', cfg, 0x18, 600, 600)
    cfg[0x1c:0x26] = bytes([0, 1, 1, 3, 0, 2, 100, 80, 0, 2])
    struct.pack_into('<III', cfg, 0x28, 0, 0, 0)
    open(os.path.join(save, 'th16.cfg'), 'wb').write(cfg)


def find_game_pid(prefix):
    # Only the game in this prefix: other copies (other prefixes) may run at
    # the same time, and their memory cannot be read anyway.
    tag = b'WINEPREFIX=' + prefix.encode() + b'\0'
    for _ in range(300):
        for name in os.listdir('/proc'):
            if not name.isdigit():
                continue
            try:
                cmd = open('/proc/%s/cmdline' % name, 'rb').read().rstrip(b'\0 ')
                if not cmd.endswith(b'th16.exe') or b':\\' not in cmd:
                    continue
                if tag not in open('/proc/%s/environ' % name, 'rb').read():
                    continue
                maps = open('/proc/%s/maps' % name).read()
            except OSError:
                continue
            if maps.startswith('00400000-') or '\n00400000-' in maps:
                return int(name)
        time.sleep(0.1)
    raise SystemExit('the game process did not appear')


def key(name):
    subprocess.run(['xdotool', 'mousemove', '300', '300', 'keydown', name, 'sleep', '0.1', 'keyup', name], check=True)


def screenshot(args, name):
    # ImageMagick's import, for checking the menu steps (--screenshots).
    if args.screenshots:
        os.makedirs(args.screenshots, exist_ok=True)
        subprocess.run(['import', '-window', 'root', os.path.join(args.screenshots, name + '.png')])


def record(args):
    prefix = os.environ.get('WINEPREFIX')
    if not prefix:
        raise SystemExit('set WINEPREFIX to a scratch Wine prefix')
    if 'DISPLAY' not in os.environ or not os.environ.get('REPLAY_REFERENCE_IN_XVFB'):
        env = dict(os.environ, REPLAY_REFERENCE_IN_XVFB='1')
        server = ['-n', str(args.display)] if args.display is not None else ['-a']
        os.execvpe('xvfb-run', ['xvfb-run'] + server + ['-s', '-screen 0 1280x1024x24', sys.executable,
                                os.path.abspath(__file__)] + sys.argv[1:], env)
    env = dict(os.environ, WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
    if not os.path.isdir(os.path.join(prefix, 'drive_c', 'users')):
        subprocess.run(['wineboot', '-i'], env=env, check=True)
    write_save_folder(prefix, args.replay)
    # The game's folder as symlinks, so that nothing is written into it.
    game = tempfile.mkdtemp(prefix='th16-reference-')
    for name in ('th16.exe', 'th16.dat', 'thbgm.dat'):
        os.symlink(os.path.join(os.path.abspath(args.game_dir), name), os.path.join(game, name))
    PR_SET_CHILD_SUBREAPER = 36
    if ctypes.CDLL(None, use_errno=True).prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0:
        raise SystemExit('prctl(PR_SET_CHILD_SUBREAPER) failed')
    log = open(args.out + '.wine.log', 'w')
    proc = subprocess.Popen(['wine', 'th16.exe'], cwd=game, env=env, stdout=log, stderr=log)
    try:
        pid = find_game_pid(prefix)
        # Ten seconds into the title menu: earlier, while the menu is still
        # coming in, key presses can get lost. The demo replay starts at 30.
        fd = os.open('/proc/%d/mem' % pid, os.O_RDONLY)
        deadline = time.time() + 180
        while struct.unpack('<i', os.pread(fd, 4, G_TITLE_IDLE_FRAMES))[0] < 600:
            if time.time() > deadline:
                raise SystemExit('the title menu did not come up')
            time.sleep(0.05)
        os.close(fd)
        screenshot(args, '0-title')
        for _ in range(args.menu_downs):
            key('Down')
            time.sleep(0.4)
        time.sleep(1)
        screenshot(args, '1-menu')
        for i in range(3):
            # Replay, the first replay, its first stage.
            key('z')
            time.sleep(3)
            screenshot(args, '%d-after-z' % (i + 2))
        sample(pid, args)
    finally:
        subprocess.run(['wineserver', '-k'], env=env)
        proc.wait()
        shutil.rmtree(game)


def sample(pid, args):
    fd = os.open('/proc/%d/mem' % pid, os.O_RDONLY)
    frames = {}
    items = {}
    lo, hi = (int(x) for x in args.items.split('-')) if args.items else (None, None)
    last_change = time.time()
    last_key = None

    def u32(addr):
        return struct.unpack('<I', os.pread(fd, 4, addr))[0]

    def i32(addr):
        return struct.unpack('<i', os.pread(fd, 4, addr))[0]

    while time.time() - last_change < 20:
        try:
            rm = u32(G_REPLAY_MANAGER)
            stage = i32(rm + REPLAY_STAGE_NUM) if rm else -1
            if not 0 <= stage < 8:
                time.sleep(0.001)
                continue
            frame_addr = rm + REPLAY_FRAME_CURRENT + REPLAY_STAGE_SIZE * stage
            frame = i32(frame_addr)
            g = os.pread(fd, GLOBALS_SIZE, G_GLOBALS)
            rng = os.pread(fd, 8, G_SAFE_RNG)
            player = u32(G_PLAYER)
            pos = os.pread(fd, 8, player + PLAYER_POS_SUBPIXEL) if player else bytes(8)
            raw = None
            if lo is not None and lo <= frame <= hi:
                raw = os.pread(fd, ITEM_SIZE * ITEM_COUNT, u32(G_ITEM_MANAGER) + ITEMS)
            if i32(frame_addr) != frame:
                continue
        except OSError:
            break
        frames[(stage, frame)] = (g, rng, pos)
        if raw is not None:
            items[frame] = raw
        if (stage, frame) != last_key:
            last_key = (stage, frame)
            last_change = time.time()
            if args.stop_after is not None and frame > args.stop_after:
                break
        time.sleep(0.001)
    with open(args.out, 'w') as f:
        for (stage, frame), (g, rng, pos) in sorted(frames.items()):
            f.write('%d %d %s %s %s\n' % (stage, frame, g.hex(), rng.hex(), pos.hex()))
            raw = items.get(frame)
            if raw is None:
                continue
            for i in range(ITEM_COUNT):
                base = i * ITEM_SIZE
                state, item_type = struct.unpack_from('<ii', raw, base + 0xc50)
                if state == 0:
                    continue
                f.write('I %d %d %d %d %d %s %s\n' % (frame, i, state, item_type,
                                                       struct.unpack_from('<i', raw, base + 0xc2c)[0],
                                                       raw[base + 0xc08:base + 0xc28].hex(),
                                                       raw[base + 0xc5c:base + 0xc60].hex()))
    print('%d frames written to %s' % (len(frames), args.out))


FIELDS = [
    'stage_num', 'weird_stage_num', 'chapter', 'time_in_stage', 'time_in_chapter', 'character', 'subshot',
    'subseason', 'score', 'difficulty', 'continues_used', 'rank', 'graze', 'graze_in_chapter', 'spell_id',
    'miss_count', 'unk_40', 'num_point_items_collected', 'piv', 'initial_piv', 'max_piv', 'power', 'max_power',
    'power_per_level', 'unk_60', 'lives', 'life_fragments', 'next_score_extend_index', 'bombs', 'bomb_fragments',
    'season_power', 'max_season_power',
] + ['season_level_deltas[%d]' % i for i in range(10)] + ['season_level_thresholds[%d]' % i for i in range(8)] + [
    'unk_c8', 'unk_cc', 'full_value_item_score', 'unk_d4', 'full_value_item_count', 'unk_dc', 'last_collect_pos.x',
    'last_collect_pos.y', 'last_collect_pos.z', 'item_spawn_count', 'enemies_spawned_in_chapter',
    'enemies_destroyed_in_chapter',
]
SKIPPED = {'time_in_stage', 'time_in_chapter'}


def decode(g, rng, pos):
    values = dict(zip(FIELDS, struct.unpack_from('<%di' % len(FIELDS), g)))
    values['music_filename'] = g[0xf8:0x1f8].split(b'\0')[0]
    for i in range(0x200, GLOBALS_SIZE, 4):
        values['unk_%x' % i] = struct.unpack_from('<i', g, i)[0]
    values['rng.seed'] = struct.unpack_from('<H', rng)[0]
    values['rng.steps'] = struct.unpack_from('<I', rng, 4)[0]
    values['player.x'], values['player.y'] = struct.unpack('<ii', pos)
    return values


def load(path):
    frames = {}
    for line in open(path):
        parts = line.split()
        if len(parts) == 5 and parts[0] != 'I':
            frames[(int(parts[0]), int(parts[1]))] = decode(*(bytes.fromhex(p) for p in parts[2:]))
    return frames


def compare(args):
    ref = load(args.reference)
    port = load(args.port)
    common = sorted(set(ref) & set(port))
    if not common:
        raise SystemExit('the dumps have no frame in common')
    print('%d frames in common (stage %d frame %d to stage %d frame %d)' % ((len(common),) + common[0] + common[-1]))
    first = {}
    for k in common:
        for name, value in ref[k].items():
            if name not in SKIPPED and name not in first and port[k][name] != value:
                first[name] = k
    if not first:
        print('no differences')
    for name, k in sorted(first.items(), key=lambda item: item[1]):
        print('%-30s stage %d frame %d: original %r, port %r' % (name, k[0], k[1], ref[k][name], port[k][name]))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    rec = sub.add_parser('record')
    rec.add_argument('--game-dir', required=True)
    rec.add_argument('--replay', required=True)
    rec.add_argument('--out', required=True)
    rec.add_argument('--items')
    rec.add_argument('--stop-after', type=int)
    rec.add_argument('--menu-downs', type=int, default=3)
    rec.add_argument('--display', type=int, help='the Xvfb display number (default: a free one)')
    rec.add_argument('--screenshots', help='save a screenshot of each menu step in this directory')
    cmp = sub.add_parser('compare')
    cmp.add_argument('reference')
    cmp.add_argument('port')
    args = parser.parse_args()
    if args.command == 'record':
        args.replay = os.path.abspath(args.replay)
        args.out = os.path.abspath(args.out)
        record(args)
    else:
        compare(args)


if __name__ == '__main__':
    main()
