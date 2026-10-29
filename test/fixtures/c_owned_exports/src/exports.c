#include <xruntime_decl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static const void *textPointer, *bytesPointer;
static int64_t released;
void* package_owned_text(void)
{ char* p = malloc(8); assert(p); memcpy(p, "package", 8); textPointer = p; return p; }
void* package_owned_bytes(void)
{ unsigned char* p = malloc(4); assert(p); memcpy(p, "\0\xff\x11*", 4); bytesPointer = p; return p; }
void package_owned_free(void* p) { assert(p); ++released; free(p); }
bool package_owned_original(xstrview text) { return text.Data == textPointer; }
bool package_owned_original_bytes(xbytesview data) { return data.Data == bytesPointer; }
int64_t package_owned_released(void) { return released; }
