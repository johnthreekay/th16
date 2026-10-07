#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef float f32;

// Integers that hold a pointer: 32 bits in the original, pointer-sized in
// the portable build (port/), where the game also runs as 64-bit code.
#ifdef TH16_PORT
typedef intptr_t iptr;
typedef uintptr_t uptr;
#else
typedef i32 iptr;
typedef u32 uptr;
#endif
