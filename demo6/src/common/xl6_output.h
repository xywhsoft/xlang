#ifndef XL6_OUTPUT_H
#define XL6_OUTPUT_H

#include "src/common/xl6_xrt.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#define XL6_C_SYMBOL_COMPONENT_MAX 48u

typedef enum XL6_OutputKind {
    XL6_OUTPUT_STD_FILE,
    XL6_OUTPUT_BUFFER_FILE,
    XL6_OUTPUT_BUFFER_STD_FILE,
    XL6_OUTPUT_BUFFER_MEMORY
} XL6_OutputKind;

typedef struct XL6_Output {
    XL6_OutputKind kind;
    FILE* stdFile;
    char* path;
    xbuffer buffer;
    char error[256];
    int bufferActive;
    int failed;
} XL6_Output;

void xl6OutputInitStdFile(XL6_Output* out, FILE* file);
int xl6OutputOpenStdFileBuffer(XL6_Output* out, FILE* file);
int xl6OutputOpenFile(XL6_Output* out, const char* path);
int xl6OutputOpenMemory(XL6_Output* out);
char* xl6OutputTakeMemoryText(XL6_Output* out, size_t* outSize);
void xl6OutputDiscard(XL6_Output* out);
void xl6OutputClose(XL6_Output* out);
int xl6OutputHasFailed(const XL6_Output* out);
const char* xl6OutputError(const XL6_Output* out);

int xl6OutputPutc(int ch, XL6_Output* out);
int xl6OutputPuts(const char* text, XL6_Output* out);
int xl6OutputVPrintf(XL6_Output* out, const char* format, va_list args);
int xl6OutputPrintf(XL6_Output* out, const char* format, ...);

/* Stable, collision-resistant encoding for one source-language component in
 * a public C symbol.  The same routine is shared by typed IR and both C
 * backends so ABI spelling cannot drift between declarations and definitions. */
int xl6OutputEncodeSymbolComponent(
    char* out, size_t capacity, const char* text);
int xl6OutputIsSimpleCIdentifier(const char* text);
int xl6OutputEncodeCIdentifier(
    char* out, size_t capacity, const char* text);

#endif
