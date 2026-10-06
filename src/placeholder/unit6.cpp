// Placeholders compiled with /GL, unlike src/stub/. Not ZUN's code: each
// forwards to an opaque stub, but because link-time code generation sees
// every caller, it shapes the calls the way it did in the original (for
// example folding an argument that is constant at every call site, so that
// callers push a junk register instead). Replace with the real function.
#include <string.h>

#include "../AnmManager.h"
#include "../SoundManager.h"


