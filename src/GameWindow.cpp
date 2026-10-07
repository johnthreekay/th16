#include "GameWindow.h"

// GLOBAL: TH16 0x4d7ce0
GameWindow g_GameWindow;

// Debug logging, compiled out of the release build. The window setup logs
// the save directories through it.
// FUNCTION: TH16 0x4595b0
void window_debug_log(const char *fmt, ...)
{
}
