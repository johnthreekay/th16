// UCRT functions that are defined inline in the CRT headers, with C linkage.
// The original kept one out-of-line copy of each; ours comes from whichever
// object file uses it. They are annotated by linker symbol because their
// definitions live in the SDK headers, not here.

// LIBRARY: TH16 0x405530 SYMBOL
// ___local_stdio_printf_options

// LIBRARY: TH16 0x405540 SYMBOL
// __vsnprintf_l

// LIBRARY: TH16 0x4090d0 SYMBOL
// _sprintf
