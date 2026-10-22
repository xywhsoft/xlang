#include "editor/lsp_coordinates.h"
#include "platform/text_utf.h"
#include <limits.h>
#include <string.h>

typedef struct reader {
    xui_code_document document;
    int at,limit,base,size;
    char buffer[4097];
} reader;

static int fill(reader* r)
{
    int take=r->limit-r->at;if(take>4096) take=4096;
    int size=-1;
    if(take<=0 || xuiCodeDocumentCopyRange(r->document,r->at,r->at+take,r->buffer,(int)sizeof(r->buffer),&size)!=XUI_OK || size!=take) return 0;
    r->base=r->at;r->size=size;return 1;
}
static int next(reader* r,int maximum,int* units)
{
    if(r->at>=r->limit || maximum<=0) return 0;
    if(r->at<r->base || r->at>=r->base+r->size) { if(!fill(r)) return 0; }
    int inside=r->at-r->base,available=r->size-inside;
    /* ASCII runs need no scalar-decoder call per byte. Keep tab/control
     * characters as single UTF-16 units, while rejecting NUL/line breaks. */
    int ascii=0;
    while(ascii<available && ascii<maximum) {
        unsigned char c=(unsigned char)r->buffer[inside+ascii];
        if(!c || c>=128 || c=='\r' || c=='\n') break;
        ++ascii;
    }
    if(ascii) { r->at+=ascii;*units=ascii;return 1; }
    /* Re-read at most three tail bytes with the next chunk. SDK CopyRange
     * accepts byte slices, so its boundary can split a UTF-8 scalar. */
    if(available<4 && available<r->limit-r->at) { if(!fill(r)) return 0;inside=0;available=r->size; }
    xstrview text={r->buffer+inside,(size_t)available};
    /* The platform adapter reuses strict system/XRT conversion and consumes
     * only complete scalars, including a bounded endpoint inside the chunk. */
    xutfresult converted=xideUtf8To16Prefix(text,maximum);
    if((converted.Status!=XUTF_OK && converted.Status!=XUTF_INVALID && converted.Status!=XUTF_NO_SPACE) ||
       !converted.Read || !converted.Written || converted.Read>(size_t)available || converted.Written>(size_t)maximum ||
       memchr(text.Data,0,converted.Read) || memchr(text.Data,'\r',converted.Read) || memchr(text.Data,'\n',converted.Read)) return 0;
    /* INVALID may describe a split tail. Consume only XRT's verified prefix;
     * the next step re-reads the failing bytes with lookahead. A real invalid
     * scalar then makes no progress and fails without publishing outputs. */
    r->at+=(int)converted.Read;*units=(int)converted.Written;return 1;
}
static int rowForOffset(xui_code_document document,int offset,int* row,int* start,int* end)
{
    int low=0,high=xuiCodeDocumentGetLineCount(document);
    while(low<high) {
        int mid=low+(high-low)/2,a=-1,b=-1;
        if(xuiCodeDocumentGetLineRange(document,mid,&a,&b)!=XUI_OK || a<0 || b<a) return 0;
        if(offset<a) high=mid;
        else if(offset>b) low=mid+1;
        else { *row=mid;*start=a;*end=b;return 1; }
    }
    return 0;
}
int xideCodeDocumentOffsetToLsp(xui_code_document document,int offset,int* line,int* character)
{
    if(!document || !line || !character || offset<0 || offset>xuiCodeDocumentGetLength(document)) return 0;
    uint32_t version=xuiCodeDocumentGetVersion(document);int row=-1,start=-1,end=-1,units=0;
    /* Use indexed byte bounds. Computing the SDK's grapheme column would
     * perform unrelated work and cannot supply the required UTF-16 column. */
    if(!rowForOffset(document,offset,&row,&start,&end)) return 0;
    reader r={.document=document,.at=start,.limit=offset};
    while(r.at<r.limit) { int add;if(!next(&r,INT_MAX-units,&add) || units>INT_MAX-add) return 0;units+=add; }
    if(xuiCodeDocumentGetVersion(document)!=version) return 0;
    *line=row;*character=units;return 1;
}
int xideCodeDocumentLspToOffset(xui_code_document document,int line,int character,int* offset)
{
    if(!document || !offset || line<0 || character<0 || line>=xuiCodeDocumentGetLineCount(document)) return 0;
    uint32_t version=xuiCodeDocumentGetVersion(document);int start=-1,end=-1,units=0;
    if(xuiCodeDocumentGetLineRange(document,line,&start,&end)!=XUI_OK || start<0 || end<start) return 0;
    reader r={.document=document,.at=start,.limit=end};
    while(units<character) { int add;if(!next(&r,character-units,&add) || add>character-units) return 0;units+=add; }
    if(xuiCodeDocumentGetVersion(document)!=version) return 0;
    *offset=r.at;return 1;
}
