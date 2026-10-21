#ifndef XLANG_IDE_TEXT_FILE_H
#define XLANG_IDE_TEXT_FILE_H
#include "app_xrt.h"
#include "model/workspace.h"
typedef struct xide_text_file {
    unsigned char* raw;
    size_t rawLength;
    char* text;
    int bom,eol;
} xide_text_file;
void xideTextFileClear(xide_text_file* file);
int xideTextFileDecode(const unsigned char* raw,size_t length,xide_text_file* file,xide_model_error* error);
int xideTextFileRead(const char* path,xide_text_file* file,xide_model_error* error);
int xideTextFileEncode(const char* text,int length,int bom,int eol,xide_text_file* file,xide_model_error* error);
#endif