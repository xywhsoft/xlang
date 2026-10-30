#include "xl6_fs_path_owner.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
static bool validPath(xstrview path,int64_t style)
{
    if(style<0||style>2){xrtSetErrorKind(XERR_RANGE);return false;}
    if(!path.Data){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(memchr(path.Data,0,path.Size)){xrtSetErrorKind(XERR_VALUE);return false;}
    return true;
}
static xstrview copyText(xstrview text)
{char* data=xrtStrDupN(text.Data?text.Data:"",text.Size);return (xstrview){data,data?text.Size:0};}
typedef struct Parts {char* text;xpathparts parts;} Parts;
void xl6_xrt_fs_path_parts_destroy(void* raw)
{Parts* p=raw;if(p){xrtFree(p->text);xrtFree(p);}}
void* xl6_xrt_fs_path_parts_create(xstrview path,int64_t style)
{
    if(!validPath(path,style))return NULL;
    Parts* p=xrtCalloc(1,sizeof(*p));if(!p)return NULL;
    p->text=xrtStrDupN(path.Data,path.Size);
    if(!p->text||!xrtPathParse((xstrview){p->text,path.Size},(xpathstyle)style,&p->parts)){
        xl6_xrt_fs_path_parts_destroy(p);return NULL;}
    return p;
}
xstrview xl6_xrt_fs_path_parts_text(void* raw,int64_t field)
{
    Parts* p=raw;xstrview text;
    if(!p){xrtSetErrorKind(XERR_ARGUMENT);return (xstrview){0};}
    switch(field){case 0:text=p->parts.Root;break;case 1:text=p->parts.Parent;break;
    case 2:text=p->parts.Name;break;case 3:text=p->parts.Stem;break;case 4:text=p->parts.Ext;break;
    default:xrtSetErrorKind(XERR_RANGE);return (xstrview){0};}
    return copyText(text);
}
int64_t xl6_xrt_fs_path_parts_number(void* raw,int64_t field)
{
    Parts* p=raw;if(!p){xrtSetErrorKind(XERR_ARGUMENT);return 0;}
    if(field==0)return p->parts.Flags;
    if(field==1)return p->parts.RootKind;
    xrtSetErrorKind(XERR_RANGE);return 0;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsPathPartsHandlePolicyV1Get,xl6_xrt_fs_path_parts_destroy)
