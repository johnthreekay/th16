// thcrap support in the portable build: what the game sources call (inside
// #ifdef TH16_PORT) where thcrap's breakpoints and binary hacks act on the
// Windows th16.exe. The implementation is port/src/thcrap/ (NOTES.md,
// "thcrap"); it reads the user's thcrap folder and run configuration and
// patches files, strings and text the way thcrap does for TH16 v1.00a.
//
// Every function is a no-op (or returns its input unchanged) when no patch
// stack is loaded, and when the port is built without thcrap support
// (TH16_THCRAP undefined: CMake option TH16_THCRAP, on when jansson and
// libpng are found), where they are the inline stand-ins below.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef TH16_THCRAP

// Whether a patch stack is loaded (port_thcrap_init found a run
// configuration whose stack has patches for th16).
bool port_thcrap_active();

// Whether the stack enables a binary hack or breakpoint of th16.v1.00a
// (one with an address, not ignored): "th15_textbox_size", "spell_align",
// "result_spell_align", "meiryo_disable", "score_force_visual_update",
// "fix_satono_1", ...
bool port_thcrap_binhack(const char *name);
bool port_thcrap_breakpoint(const char *name);

// --- Files (FileSystem.cpp, file_read_all; thcrap's file_size, file_load
// and file_loaded breakpoints) ---

// A patch's replacement for the archive file `name`, or NULL. The buffer
// (malloc) has room for what the format patchers may add; *size is the
// buffer size.
uint8_t *port_thcrap_file_replacement(const char *name, uint32_t *size);
// Runs the format patchers on a file as read from the archive or replaced:
// .msg dialogue and endings (jdiff), .anm (PNG images, header jdiff).
// `data` is malloc'ed with `*size` bytes; returns the (possibly
// reallocated) buffer and updates *size.
uint8_t *port_thcrap_patch_file(const char *name, uint8_t *data, uint32_t *size);

// --- Strings (thcrap's sprintf binary hacks: strings_sprintf) ---

// vsnprintf into buf after translating the format and every %s argument
// that is a string the stack's stringlocs/stringdefs translate. Returns the
// result's length like vsnprintf. Without a stack: plain vsnprintf.
int port_thcrap_vsnprintf(char *buf, size_t size, const char *format, va_list args);
int port_thcrap_snprintf(char *buf, size_t size, const char *format, ...);
// The translation of a string the game uses (strings_lookup), or `text`.
const char *port_thcrap_string(const char *text);

// --- Spell cards (spell_id, spell_name breakpoints; spells.js) ---

// ECL spell instruction: the spell id before the difficulty is added.
void port_thcrap_spell_id(int32_t spell_id);
// Spellcard::start: the name to show for spell `real_id` (looked up from
// real_id down to the id the ECL gave), or `name`.
const char *port_thcrap_spell_name(int32_t real_id, const char *name);
// Result screen and spell practice: the name for `real_id`, looked up from
// real_id down to real_id - rank.
const char *port_thcrap_spell_name_ranked(int32_t real_id, int32_t rank, const char *name);

// --- Music room (music_title, music_cmt breakpoints; themes.js,
// musiccmt.js) ---

// The title row of track `track` (0-based), or `title`.
const char *port_thcrap_music_title(int32_t track, const char *title);
// Line `line` of the comment of track `track` (0-based), or `comment`.
const char *port_thcrap_music_comment(int32_t track, int32_t line, const char *comment);

// --- Dialogue (GuiMsgVm::run) ---

// ruby_offset: furigana "|x,y,text" whose text is in thcrap's syntax
// ("|\tbefore\t,\tbase\t,ruby") get their offset computed from the text
// widths. `params` points after the '|'; it may be moved so that the
// game's two strchr(',') calls find the ruby text. Returns the x to use.
int32_t port_thcrap_ruby_offset(const char **params, int32_t x);
// th15_textbox_size: the speech bubble's width for a line, from the width
// of the text in the dialogue font instead of its length in bytes.
// `width` is the game's own value; returned unchanged without the hack.
float port_thcrap_textbox_width(const char *text, float width);

// --- Text placement (AnmText.cpp; spell_align, result_spell_align) ---

// draw_text_right: the x for right-aligned text, or `x`.
int32_t port_thcrap_text_right_x(const char *text, int32_t font, float sprite_width, int32_t x);
// draw_text_centered: the x for the result screen's centered text, or `x`.
int32_t port_thcrap_text_centered_x(int32_t x);

#else

static inline bool port_thcrap_active()
{
    return false;
}
static inline bool port_thcrap_binhack(const char *)
{
    return false;
}
static inline bool port_thcrap_breakpoint(const char *)
{
    return false;
}
static inline uint8_t *port_thcrap_file_replacement(const char *, uint32_t *)
{
    return NULL;
}
static inline uint8_t *port_thcrap_patch_file(const char *, uint8_t *data, uint32_t *)
{
    return data;
}
static inline int port_thcrap_vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    return vsnprintf(buf, size, format, args);
}
static inline int port_thcrap_snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buf, size, format, args);
    va_end(args);
    return result;
}
static inline const char *port_thcrap_string(const char *text)
{
    return text;
}
static inline void port_thcrap_spell_id(int32_t)
{
}
static inline const char *port_thcrap_spell_name(int32_t, const char *name)
{
    return name;
}
static inline const char *port_thcrap_spell_name_ranked(int32_t, int32_t, const char *name)
{
    return name;
}
static inline const char *port_thcrap_music_title(int32_t, const char *title)
{
    return title;
}
static inline const char *port_thcrap_music_comment(int32_t, int32_t, const char *comment)
{
    return comment;
}
static inline int32_t port_thcrap_ruby_offset(const char **, int32_t x)
{
    return x;
}
static inline float port_thcrap_textbox_width(const char *, float width)
{
    return width;
}
static inline int32_t port_thcrap_text_right_x(const char *, int32_t, float, int32_t x)
{
    return x;
}
static inline int32_t port_thcrap_text_centered_x(int32_t x)
{
    return x;
}

#endif
