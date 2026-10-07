// Placeholders for functions other units own whose call sites only take
// their original shape under LTCG's custom conventions (float arguments in
// xmm registers, constant arguments folded into the callee). Unlike
// src/stub/, this file is compiled with /GL and nothing here is forced
// alive with /INCLUDE, so LTCG sees every caller and converts these the way
// it converted the real functions. Each body hands its arguments to an
// opaque stub so the calls cannot be optimized away.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../Player.h"
#include "../SoundManager.h"
#include "../ZunTimer.h"

void placeholder_sink(int a, float b);



