#ifndef XL6_C_PRINT_H
#define XL6_C_PRINT_H
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
/* No xvalue is manufactured for a native text/binary loan. Other dynamic
 * arguments are formatted before the first write, preserving evaluation order. */
extern xstrview xl6ValueTextV1(const xvalue*);
typedef struct XL6_CPrintPartV1 {
    int Kind; /* 1 text, 2 binary, 3 ordinary dynamic value */
    const void* Data;
    size_t Size;
    const xvalue* Value;
    xstrview Rendered;
    xxlonwriter* Writer;
} XL6_CPrintPartV1;
static inline bool xl6CPrintSinkV1(xbytesview text,void* context)
{
    (void)context;
    return xrtConsoleWrite(XCONSOLE_STDOUT,(xstrview){(const char*)text.Data,text.Size});
}
static inline bool xl6CPrintV1(XL6_CPrintPartV1* parts,size_t count,bool newline)
{
    bool ready=true;
    for(size_t i=0;ready && i<count;++i) {
        XL6_CPrintPartV1* part=&parts[i];
        if(part->Kind==3) {
            part->Rendered=xl6ValueTextV1(part->Value);
            ready=part->Rendered.Data!=NULL;
            part->Data=part->Rendered.Data;part->Size=part->Rendered.Size;
        } else if(part->Kind==2) {
            xxlonwriteconfig config; xrtXlonWriteConfigInit(&config);
            if(part->Size>SIZE_MAX-2 || (part->Size+2)/3>(SIZE_MAX-9)/4 ||
                config.MaxOutputBytes<9 || ((part->Size+2)/3)*4>config.MaxOutputBytes-9) {
                xrtSetErrorKind(XERR_RANGE);ready=false;break;
            }
            part->Writer=xrtXlonWriterCreateSink(&config,xl6CPrintSinkV1,NULL);
            ready=part->Writer!=NULL;
        }
    }
    for(size_t i=0;ready && i<count;++i) {
        XL6_CPrintPartV1* part=&parts[i];
        if(i)ready=xrtConsoleWrite(XCONSOLE_STDOUT,(xstrview){" ",1});
        if(!ready)break;
        ready=part->Kind==2
            ? xrtXlonWriterBytes(part->Writer,(xbytesview){(const uint8*)part->Data,part->Size}) && xrtXlonWriterFinish(part->Writer)
            : xrtConsoleWrite(XCONSOLE_STDOUT,(xstrview){part->Data ? (const char*)part->Data : "",part->Size});
    }
    if(ready && newline)ready=xrtConsoleWriteLine(XCONSOLE_STDOUT,(xstrview){"",0});
    for(size_t i=count;i>0;--i) {
        xrtFree((void*)parts[i-1].Rendered.Data);
        xrtXlonWriterFree(parts[i-1].Writer);
    }
    if(!ready && !xrtGetError())xrtSetErrorKind(XERR_IO);
    return ready;
}
#endif
