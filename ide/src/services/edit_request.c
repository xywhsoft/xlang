#include "services/edit_request.h"
#include <string.h>
#include <stdio.h>
struct xide_edit_request {
 volatile int32 references;xatomic32 reasons;uint64_t token;int request;
 size_t count,bytes;xls_sync* sync;xvalue* catalog;
 xide_edit_request_model models[];
};
static int fail(xide_model_error* e,int code,const char* text)
{if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static int same(const char* a,const char* b){return a&&b?!strcmp(a,b):a==b;}
xide_edit_request* xideEditRequestCreate(uint64_t token,int request,const xls_sync* sync,xvalue* catalog,size_t count,int(*modelAt)(void*,size_t,xide_edit_request_model*),void* user,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!token||request<=0||!sync||!xrtValueIs(catalog,XVALUE_OBJECT)||count>4096||(count&&!modelAt)){fail(e,XIDE_MODEL_INVALID,"Invalid edit request scope.");return NULL;}
 size_t bytes=sizeof(xide_edit_request)+count*sizeof(xide_edit_request_model);
 for(size_t i=0;i<count;++i){xide_edit_request_model m={0};if(!modelAt(user,i,&m)||(m.open&&(!m.identity||!m.revision))){fail(e,XIDE_MODEL_INVALID,"Cannot capture a complete live model roster.");return NULL;}
  const char* strings[]={m.path,m.language};for(size_t j=0;j<2;++j)if(strings[j]){size_t n=strlen(strings[j])+1;if(n>65536||n>1024u*1024u-bytes){fail(e,XIDE_MODEL_MEMORY,"Edit request model metadata limit exceeded.");return NULL;}bytes+=n;}}
 xide_edit_request* p=xrtCalloc(1,bytes);if(!p){fail(e,XIDE_MODEL_MEMORY,"Cannot allocate edit request metadata.");return NULL;}
 p->references=1;xrtAtomic32Init(&p->reasons,0);p->token=token;p->request=request;p->count=count;p->bytes=bytes;char* next=(char*)(p->models+count);size_t left=bytes-(size_t)(next-(char*)p);int okay=1;
 for(size_t i=0;okay&&i<count;++i){xide_edit_request_model m={0};okay=modelAt(user,i,&m)&&(!m.open||(m.identity&&m.revision));p->models[i]=m;
  const char* strings[]={m.path,m.language};const char** targets[]={&p->models[i].path,&p->models[i].language};for(size_t j=0;okay&&j<2;++j)if(strings[j]){size_t n=strlen(strings[j])+1;if(n>left){okay=0;break;}memcpy(next,strings[j],n);*targets[j]=next;next+=n;left-=n;}}
 if(okay){p->sync=xlsSyncClone(sync);okay=p->sync!=NULL;}
 if(okay){p->catalog=xrtValueRetain(catalog);okay=p->catalog!=NULL;}
 if(!okay){xideEditRequestRelease(p);fail(e,XIDE_MODEL_MEMORY,"Cannot retain a complete immutable edit request.");return NULL;}return p;
}
xide_edit_request* xideEditRequestRetain(xide_edit_request* p)
{return p&&xrtRefRetain(&p->references)>0?p:NULL;}
void xideEditRequestRelease(xide_edit_request* p)
{if(p&&xrtRefRelease(&p->references)==0){xlsSyncDestroy(p->sync);xrtValueRelease(p->catalog);xrtFree(p);}}
void xideEditRequestInvalidate(xide_edit_request* p,unsigned reasons)
{if(p&&reasons)xrtAtomic32FetchOr(&p->reasons,(int32)reasons,XMEMORY_RELEASE);}
unsigned xideEditRequestInvalidated(const xide_edit_request* p)
{return p?(unsigned)xrtAtomic32Load(&p->reasons,XMEMORY_ACQUIRE):XIDE_EDIT_REQUEST_ENDED;}
uint64_t xideEditRequestToken(const xide_edit_request* p){return p?p->token:0;}
int xideEditRequestId(const xide_edit_request* p){return p?p->request:0;}
size_t xideEditRequestModelCount(const xide_edit_request* p){return p?p->count:0;}
int xideEditRequestModelMatches(const xide_edit_request* p,size_t i,const xide_edit_request_model* m)
{if(!p||!m||i>=p->count)return 0;const xide_edit_request_model* a=p->models+i;return a->identity==m->identity&&a->revision==m->revision&&a->open==m->open&&a->dirty==m->dirty&&a->external==m->external&&same(a->path,m->path)&&same(a->language,m->language);}
const xls_sync* xideEditRequestSync(const xide_edit_request* p){return p?p->sync:NULL;}
const xvalue* xideEditRequestCatalog(const xide_edit_request* p){return p?p->catalog:NULL;}
size_t xideEditRequestMetadata(const xide_edit_request* p){return p?p->bytes:0;}
