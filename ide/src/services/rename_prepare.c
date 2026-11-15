#include "services/rename_prepare.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define PREFIX "xide.prepareRename/"
struct xide_rename_prepare {
 xmutex lock;uint64_t serial,active;int received,ready;
 char id[40];xide_rename_prepared completed;void (*wake)(void*);void* user;
};
static const xvalue* get(const xvalue* v,const char* k){return xrtValueType(v)==XVALUE_OBJECT?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int position(const xvalue* v,xide_lsp_position* p)
{
 int64 line=0,character=0;
 if(xrtValueType(v)!=XVALUE_OBJECT || xrtValueCount(v)!=2 || xrtValueType(get(v,"line"))!=XVALUE_INT ||
    xrtValueType(get(v,"character"))!=XVALUE_INT || !xrtValueGetInt(get(v,"line"),&line) || !xrtValueGetInt(get(v,"character"),&character) ||
    line<0 || line>INT_MAX || character<0 || character>INT_MAX)return 0;
 *p=(xide_lsp_position){.line=(int)line,.character=(int)character,.offset=-1};return 1;
}
void xideRenamePreparedClear(xide_rename_prepared* r)
{if(r){xrtValueRelease(r->placeholder);memset(r,0,sizeof(*r));}}
xide_rename_prepare* xideRenamePrepareCreate(void (*wake)(void*),void* user)
{xide_rename_prepare* q=xrtCalloc(1,sizeof(*q));if(!q)return NULL;if(!xrtMutexInit(&q->lock)){xrtFree(q);return NULL;}q->wake=wake;q->user=user;return q;}
uint64_t xideRenamePrepareArm(xide_rename_prepare* q,char* id,size_t capacity)
{
 if(!q || !id || capacity<40)return 0;
 xrtMutexLock(&q->lock);uint64_t token=0;
 if(q->serial!=UINT64_MAX){token=++q->serial;q->active=token;q->received=q->ready=0;xideRenamePreparedClear(&q->completed);
  snprintf(q->id,sizeof(q->id),PREFIX "%016llx",(unsigned long long)token);memcpy(id,q->id,strlen(q->id)+1);}
 xrtMutexUnlock(&q->lock);return token;
}
static void decode(const xvalue* m,size_t bytes,xide_rename_prepared* out)
{
 snprintf(out->error,sizeof(out->error),"语言服务返回了无效的重命名准备结果。");
 xstrview rpc={0},name={0};const xvalue *result=get(m,"result"),*range=get(result,"range"),*placeholder=get(result,"placeholder"),*error=get(m,"error");
 if(bytes>512u*1024u || xrtValueCount(m)!=3 || !xrtValueGetString(get(m,"jsonrpc"),&rpc) || rpc.Size!=3 || memcmp(rpc.Data,"2.0",3))return;
 if(error){
  xstrview reason={0};int64 code=0;
  if(!result && xrtValueType(error)==XVALUE_OBJECT && xrtValueType(get(error,"code"))==XVALUE_INT && xrtValueGetInt(get(error,"code"),&code) &&
     xrtValueGetString(get(error,"message"),&reason) && reason.Size && reason.Size<=240 && !memchr(reason.Data,0,reason.Size) && xrtUtf8Valid(reason,NULL))
   snprintf(out->error,sizeof(out->error),"%.*s",(int)reason.Size,reason.Data);
  return;
 }
 if(xrtValueType(result)!=XVALUE_OBJECT || xrtValueCount(result)!=2 || xrtValueType(range)!=XVALUE_OBJECT || xrtValueCount(range)!=2 ||
    !position(get(range,"start"),&out->start) || !position(get(range,"end"),&out->end) ||
    out->start.line!=out->end.line || out->start.character>=out->end.character ||
    !xrtValueGetString(placeholder,&name) || !name.Size || name.Size>65535 || memchr(name.Data,0,name.Size) || !xrtUtf8Valid(name,NULL))return;
 out->placeholder=xrtValueRetain(placeholder);out->valid=1;*out->error=0;
}
int xideRenamePrepareReceive(xide_rename_prepare* q,xvalue* m,size_t bytes)
{
 xstrview id={0};if(!q || !m || xrtValueType(get(m,"method"))==XVALUE_STRING || !xrtValueGetString(get(m,"id"),&id) || id.Size<sizeof(PREFIX)-1 || memcmp(id.Data,PREFIX,sizeof(PREFIX)-1))return 0;
 xrtMutexLock(&q->lock);uint64_t token=q->active;
 int match=token && !q->received && id.Size==strlen(q->id) && !memcmp(id.Data,q->id,id.Size);
 if(match)q->received=1;
 xrtMutexUnlock(&q->lock);
 xide_rename_prepared out={.token=token};if(match)decode(m,bytes,&out);
 /* Transport already bounds/decodes frames. Retirement stays on that worker,
    including hostile oversized DOMs; no owner-side deep walk or release. */
 xrtValueRelease(m);int notify=0;
 xrtMutexLock(&q->lock);
 if(match && q->active==token && !q->ready){q->completed=out;memset(&out,0,sizeof(out));q->ready=notify=1;}
 xrtMutexUnlock(&q->lock);xideRenamePreparedClear(&out);if(notify && q->wake)q->wake(q->user);return 1;
}
int xideRenamePrepareTake(xide_rename_prepare* q,xide_rename_prepared* out)
{if(!q || !out)return 0;xrtMutexLock(&q->lock);int ready=q->ready;if(ready){*out=q->completed;memset(&q->completed,0,sizeof(q->completed));q->ready=0;}xrtMutexUnlock(&q->lock);return ready;}
int xideRenamePreparedSpan(const xide_rename_prepared* r,xide_lsp_position point,int caret,int documentLength,int* start,int* end)
{
 xstrview name={0};if(!r || !r->valid || !start || !end || caret<0 || caret>documentLength ||
    r->start.line<0 || point.line!=r->start.line || r->end.line!=r->start.line || r->start.character<0 ||
    r->end.character<=r->start.character || point.character<r->start.character || point.character>r->end.character ||
    !xrtValueGetString(r->placeholder,&name) || !name.Size || name.Size>65535 || !xrtUtf8Valid(name,NULL))return 0;
 int delta=point.character-r->start.character,units=0,relative=-1;
 for(size_t i=0;i<name.Size;){
  unsigned char c=(unsigned char)name.Data[i];if(!c || c=='\r' || c=='\n')return 0;
  if(units==delta)relative=(int)i;
  size_t width=c<0x80?1:c<0xe0?2:c<0xf0?3:4;units+=width==4?2:1;i+=width;
 }
 if(units==delta)relative=(int)name.Size;
 if(relative<0 || relative>caret || units!=r->end.character-r->start.character)return 0;
 int first=caret-relative;if(name.Size>(size_t)(documentLength-first))return 0;
 *start=first;*end=first+(int)name.Size;return 1;
}
void xideRenamePrepareCancel(xide_rename_prepare* q)
{if(q){xrtMutexLock(&q->lock);q->active=0;q->ready=q->received=0;xideRenamePreparedClear(&q->completed);xrtMutexUnlock(&q->lock);}}
void xideRenamePrepareDestroy(xide_rename_prepare* q)
{if(q){xideRenamePrepareCancel(q);xrtMutexUnit(&q->lock);xrtFree(q);}}
