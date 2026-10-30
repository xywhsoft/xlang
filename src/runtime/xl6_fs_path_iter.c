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
typedef struct Cursor {char* text;xpathiter iter;xpathcomponent item;bool current;} Cursor;
void xl6_xrt_fs_path_iter_destroy(void* raw)
{Cursor* p=raw;if(p){xrtFree(p->text);xrtFree(p);}}
void* xl6_xrt_fs_path_iter_create(xstrview path,int64_t style)
{
    if(!validPath(path,style))return NULL;
    Cursor* p=xrtCalloc(1,sizeof(*p));if(!p)return NULL;
    p->text=xrtStrDupN(path.Data,path.Size);
    if(!p->text||!xrtPathIterInit(&p->iter,(xstrview){p->text,path.Size},(xpathstyle)style)){
        xl6_xrt_fs_path_iter_destroy(p);return NULL;}
    return p;
}
bool xl6_xrt_fs_path_iter_advance(void* raw)
{
    Cursor* p=raw;if(!p){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    p->current=xrtPathNext(&p->iter,&p->item);return p->current;
}
xstrview xl6_xrt_fs_path_iter_text(void* raw)
{
    Cursor* p=raw;if(!p||!p->current){xrtSetErrorKind(XERR_STATE);return (xstrview){0};}
    return copyText(p->item.Text);
}
int64_t xl6_xrt_fs_path_iter_kind(void* raw)
{Cursor* p=raw;if(!p||!p->current){xrtSetErrorKind(XERR_STATE);return 0;}return p->item.Kind;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsPathIterHandlePolicyV1Get,xl6_xrt_fs_path_iter_destroy)
