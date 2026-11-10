#ifndef XLS_IDENTIFIER_H
#define XLS_IDENTIFIER_H
#include <stddef.h>
#define XLS_IDENTIFIER_BYTES 65535u
/* Exact native lexer IDENT rule + original keyword lookup, on strict UTF-8.
 * No scanner, allocation, copy, parsing or locale state. Thread-safe. A lexical
 * identifier is NOT proof that a rename is writable, conflict-free or preserves
 * bindings. Builtin type spellings may be IDENTs; semantic policy is separate. */
int xlsIdentifierValid(const char* text,size_t length);
#endif
