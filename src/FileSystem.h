#pragma once

#include <windows.h>

#include "decomp.h"
#include "types.h"

// Whole-file write. Returns 0, -1 if the file cannot be created, or -2 on a
// short write.
i32 LTCG_FASTCALL file_write(const char *path, const void *data, i32 size);
BOOL LTCG_FASTCALL file_exists(const char *path);

// Streaming access through one shared handle. Opening takes the file
// critical section and file_close releases it, so only one file is open
// this way at a time.
i32 LTCG_FASTCALL file_create(const char *path);
i32 LTCG_FASTCALL file_open(const char *path);
u8 *LTCG_FASTCALL file_read(i32 size);
i32 file_close();
