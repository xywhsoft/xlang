#ifndef XLANG_PROTOCOL_INPUT_STATE_H
#define XLANG_PROTOCOL_INPUT_STATE_H
#include <stddef.h>
#include <stdint.h>

/* Exact wire URI spelling and signed LSP version, not source text or semantic
   validity. Hash each URI once at open; aggregate at most 64 fixed-size rows. */
typedef struct xls_input_state_item { unsigned char uri[32]; int32_t version; } xls_input_state_item;
int xlsInputStateUri(const char* uri,unsigned char hash[32]);
/* Order-independent, domain-separated SHA-256; duplicate URI hashes reject.
   No allocation, source access or I/O. Clears output on failure. */
int xlsInputStateDigest(const xls_input_state_item* items,size_t count,char digest[65]);
#endif
