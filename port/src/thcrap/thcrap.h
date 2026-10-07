// The port's own calls into its thcrap support (main.cpp, gdi_text.cpp);
// the game's are in port/include/port_thcrap.h. Only with TH16_THCRAP.
#pragma once

#include <windows.h>

// Loads the patch stack: the thcrap folder is `dir` (--thcrap), else
// $TH16_THCRAP_DIR, else $XDG_DATA_HOME/thcrap or ~/.local/share/thcrap
// if it exists; the run configuration is `config` (--thcrap-config: a
// path, or a name in the folder's config/), else $TH16_THCRAP_CONFIG,
// else the newest run configuration in config/. TH16_THCRAP=0 turns it
// off. Returns whether a stack with patches for th16 was loaded.
bool port_thcrap_init(const char *dir, const char *config);

// gdi_text.cpp's CreateFontA: thcrap's textdisp_CreateFontA (the run
// configuration's "font" replaces the face).
void port_thcrap_font_face(char face[LF_FACESIZE]);
// gdi_text.cpp's CreateFontIndirectA: the "fontrules", and
// DEFAULT_CHARSET for named faces (textdisp_CreateFontIndirectExA).
void port_thcrap_font_rules(LOGFONTA *lf);
// gdi_text.cpp's TextOutA: string lookup, layout markup and TL note
// removal (layout_TextOutU), then port_gdi_text_out_raw. Returns false
// when it did not handle the call.
bool port_thcrap_text_out(HDC hdc, int x, int y, const char *text, int length);
