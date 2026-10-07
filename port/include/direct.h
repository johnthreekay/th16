// Stand-in for the MSVC CRT's direct.h: _chdir, _mkdir and _getcwd. The
// game passes Windows-style relative paths; the implementation maps them to
// the host's separators.
#pragma once

extern "C" {
int _chdir(const char *dirname);
int _mkdir(const char *dirname);
char *_getcwd(char *buffer, int maxlen);
}
