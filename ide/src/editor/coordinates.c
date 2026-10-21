#include "editor/coordinates.h"
#include <string.h>
static size_t next(const char* text,size_t at,size_t size)
{
    unsigned char c=(unsigned char)text[at]; size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
    return at+n<=size ? at+n : size;
}
void xidePositionFromOffset(const char* text,int offset,int* line,int* column)
{
    size_t size=strlen(text),limit=offset<0 ? 0 : (size_t)offset>size ? size : (size_t)offset,at=0; int l=1,c=1;
    while(at<limit) { size_t end=next(text,at,size); if(end>limit) break; if(text[at]=='\n') { ++l; c=1; } else ++c; at=end; }
    *line=l; *column=c;
}
int xidePositionToOffset(const char* text,int line,int column)
{
    size_t size=strlen(text),at=0; if(line<1) line=1; if(column<1) column=1;
    while(at<size && line>1) { if(text[at]=='\n') --line; at=next(text,at,size); }
    while(at<size && text[at]!='\n' && column>1) { at=next(text,at,size); --column; }
    return (int)at;
}