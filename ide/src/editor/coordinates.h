#ifndef XLANG_IDE_COORDINATES_H
#define XLANG_IDE_COORDINATES_H
/* UTF-8 byte offsets <-> one-based lines and Unicode code-point columns. */
void xidePositionFromOffset(const char* text,int offset,int* line,int* column);
int xidePositionToOffset(const char* text,int line,int column);
#endif