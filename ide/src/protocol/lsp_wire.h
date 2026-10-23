#ifndef XLANG_IDE_LSP_WIRE_H
#define XLANG_IDE_LSP_WIRE_H
#include "model/workspace.h"
typedef struct xide_lsp_stream { char* bytes; size_t length,capacity,body,header; int failed; } xide_lsp_stream;
typedef int (*xide_lsp_message_proc)(const xvalue* message,void* user);
/* Success TAKES the parsed root; rejection takes nothing and the stream frees
 * it. bodyLength is the exact UTF-8 Content-Length, including JSON whitespace.
 * No borrowed pointers into the stream buffer survive either callback. */
typedef int (*xide_lsp_owned_message_proc)(xvalue* message,size_t bodyLength,void* user);
void xideLspStreamClear(xide_lsp_stream* stream);
int xideLspStreamFeed(xide_lsp_stream* stream,const void* bytes,size_t length,xide_lsp_message_proc receive,void* user,xide_model_error* error);
int xideLspStreamFeedOwned(xide_lsp_stream* stream,const void* bytes,size_t length,xide_lsp_owned_message_proc receive,void* user,xide_model_error* error);
int xideLspStreamEnd(xide_lsp_stream* stream,xide_model_error* error);
char* xideLspFrame(const xvalue* message,size_t* length,xide_model_error* error);
char* xideLspPathToUri(const char* absolutePath,xide_model_error* error);
char* xideLspUriToPath(const char* uri,xide_model_error* error);
/* Strict UTF-16 positions; reject malformed UTF-8, out-of-range positions and
   offsets inside a scalar or CRLF. Returned editor offsets are UTF-8 bytes. */
typedef struct xide_lsp_position { int line,character,offset; } xide_lsp_position;
/* Validate and map an unordered batch in one text traversal. All offsets are
   committed only if every position is valid. */
int xideLspPositionsToOffsets(const char* text,xide_lsp_position* positions,size_t count);
/* Background large-response variant: at most 131072 positions. The callback
 * may reject during mapping; no offsets are published on failure/cancellation.
 * The ordinary UI batch keeps its original 8192-position bound. */
int xideLspManyPositionsToOffsets(const char* text,xide_lsp_position* positions,size_t count,
                                int (*cancelled)(void*),void* user);
int xideLspPositionToOffset(const char* text,int line,int character,int* offset);
int xideLspOffsetToPosition(const char* text,int offset,int* line,int* character);
#endif
