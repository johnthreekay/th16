// UCRT functions that are defined inline in the CRT headers, with C linkage.
// The original kept one out-of-line copy of each; ours comes from whichever
// object file uses it. They are annotated by linker symbol because their
// definitions live in the SDK headers, not here.

// LIBRARY: TH16 0x405260 SYMBOL
// _floorf

// LIBRARY: TH16 0x4054f0 SYMBOL
// _cosf

// LIBRARY: TH16 0x405510 SYMBOL
// _sinf

// LIBRARY: TH16 0x405530 SYMBOL
// ___local_stdio_printf_options

// LIBRARY: TH16 0x405540 SYMBOL
// __vsnprintf_l

// LIBRARY: TH16 0x4090d0 SYMBOL
// _sprintf

// LIBRARY: TH16 0x43dcb0 SYMBOL
// ___local_stdio_scanf_options
