#pragma once

// ZUN's debug log. Release builds compile it to an empty function, but MSVC
// never inlines variadic functions, so every call is still made.
void zun_log(const char *fmt, ...);
