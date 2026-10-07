// Placeholders compiled with /GL for functions that wave 3 range B
// (0x4190b0-0x42b480) calls, where LTCG has to see a body (see
// src/placeholder/unit2.cpp). Each forwards to an opaque stub.
#include <stdarg.h>
#include <stdio.h>

#include "../AnmManager.h"
#include "../Interp.h"

int w3b_placeholder_sink(void *object, int value);

