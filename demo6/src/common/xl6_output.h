#ifndef XL6_OUTPUT_H
#define XL6_OUTPUT_H

#include "lib/xrt/xrt.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

typedef enum XL6_OutputKind {
    XL6_OUTPUT_STD_FILE,
    XL6_OUTPUT_BUFFER_FILE,
    XL6_OUTPUT_BUFFER_STD_FILE
} XL6_OutputKind;

typedef struct XL6_Output {
    XL6_OutputKind kind;
    FILE* stdFile;
    char* path;
    xbuffer buffer;
    const char* error;
    int failed;
} XL6_Output;

void xl6OutputInitStdFile(XL6_Output* out, FILE* file);
int xl6OutputOpenStdFileBuffer(XL6_Output* out, FILE* file);
int xl6OutputOpenFile(XL6_Output* out, const char* path);
void xl6OutputClose(XL6_Output* out);
int xl6OutputHasFailed(const XL6_Output* out);
const char* xl6OutputError(const XL6_Output* out);

int xl6OutputPutc(int ch, XL6_Output* out);
int xl6OutputPuts(const char* text, XL6_Output* out);
int xl6OutputVPrintf(XL6_Output* out, const char* format, va_list args);
int xl6OutputPrintf(XL6_Output* out, const char* format, ...);

#endif
