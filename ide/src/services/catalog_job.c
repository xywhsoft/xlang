#include "services/catalog_job.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct xide_catalog_job { volatile LONG references,cancelled,done; HANDLE thread; char* path; xide_catalog_result result; };
void xideCatalogResultClear(xide_catalog_result* r)
{
    if(!r)return;
    for(size_t i=0;r->details && i<r->catalog.count;++i)free(r->details[i]);
    free(r->details);free(r->nodes);xideCatalogLookupDestroy(r->lookup);xideCatalogClear(&r->catalog);memset(r,0,sizeof(*r));
}
static void release(xide_catalog_job* j)
{if(j && !InterlockedDecrement(&j->references)){if(j->thread)CloseHandle(j->thread);xideCatalogResultClear(&j->result);free(j->path);free(j);}}
int xideCatalogTreePrepare(const xide_command_catalog* c,const char* query,xui_tree_view_node_t** output,int* count,size_t* visible)
{
    if(!c || !query || !output || !count || !visible)return XUI_ERROR_INVALID_ARGUMENT;
    size_t capacity=8;while(capacity<c->count*2+1)capacity*=2;
    int* slots=calloc(capacity,sizeof(*slots));
    xui_tree_view_node_t* nodes=calloc(c->count*2+3,sizeof(*nodes));
    if(!slots || !nodes){free(slots);free(nodes);return XUI_ERROR_OUT_OF_MEMORY;}
    const char* names[]={"语言结构","标准库与基础命令","扩展库"};
    for(int i=0;i<3;++i)nodes[i]=(xui_tree_view_node_t){.iId=i+1,.iParent=-1,.sText=names[i],.bExpanded=*query || i==0,.bEnabled=1};
    int n=3,groups=0;size_t matches=0;
    for(size_t i=0;i<c->count;++i){
        const xide_command* command=&c->entries[i];if(!xideCatalogMatches(command,query))continue;
        int category=!strcmp(command->category,"language")?1:!strcmp(command->category,"standard")?2:3;
        uint64_t h=(uint64_t)category;for(const unsigned char* s=(const unsigned char*)command->module;*s;++s)h=(h^*s)*UINT64_C(1099511628211);
        size_t slot=(size_t)h&(capacity-1);
        while(slots[slot] && (nodes[slots[slot]].iParent!=category || strcmp(nodes[slots[slot]].sText,command->module)))slot=(slot+1)&(capacity-1);
        if(!slots[slot]){slots[slot]=n;nodes[n++]=(xui_tree_view_node_t){.iId=100000+groups++,.iParent=category,.sText=command->module,.bExpanded=*query || category==1,.bEnabled=1};}
        nodes[n++]=(xui_tree_view_node_t){.iId=100+(int)i,.iParent=nodes[slots[slot]].iId,.sText=command->name,.bEnabled=1};++matches;
    }
    free(slots);*output=nodes;*count=n;*visible=matches;return XUI_OK;
}
static DWORD WINAPI work(void* raw)
{
    xide_catalog_job* j=raw;xide_catalog_result* r=&j->result;xide_model_error e={0};ULONGLONG start=GetTickCount64();
    if(InterlockedCompareExchange(&j->cancelled,0,0))goto done;
    if(!xideCatalogLoad(&r->catalog,j->path,&e)){snprintf(r->error,sizeof(r->error),"%s",e.message);goto done;}
    r->details=calloc(r->catalog.count+1,sizeof(*r->details));if(!r->details)goto memory;
    for(size_t i=0;i<r->catalog.count;++i){
        if(InterlockedCompareExchange(&j->cancelled,0,0))goto done;
        const xide_command* c=&r->catalog.entries[i];size_t n=strlen(c->signature)+strlen(c->requiredImport)+48;
        r->details[i]=malloc(n);if(!r->details[i])goto memory;
        snprintf(r->details[i],n,"%s%s%s",c->signature,*c->requiredImport?" · 需 import ":"",c->requiredImport);
    }
    r->lookup=xideCatalogLookupCreate(&r->catalog);
    if(!r->lookup || xideCatalogTreePrepare(&r->catalog,"",&r->nodes,&r->nodeCount,&r->visible)!=XUI_OK)goto memory;
    goto done;
memory:snprintf(r->error,sizeof(r->error),"命令目录准备内存不足。");
done:xrtErrorFree(xrtTakeError());r->loadMs=(double)(GetTickCount64()-start);InterlockedExchange(&j->done,1);release(j);return 0;
}
xide_catalog_job* xideCatalogJobStart(const char* path)
{
    if(!path)return NULL;
    xide_catalog_job* j=calloc(1,sizeof(*j));if(!j)return NULL;
    j->references=2;j->path=malloc(strlen(path)+1);if(!j->path){free(j);return NULL;}strcpy(j->path,path);
    /* The caller reference pins the job even when work finishes immediately. */
    j->thread=CreateThread(NULL,0,work,j,0,NULL);
    if(!j->thread){j->references=1;release(j);return NULL;}
    SetThreadPriority(j->thread,THREAD_PRIORITY_BELOW_NORMAL);return j;
}
int xideCatalogJobTake(xide_catalog_job* j,xide_catalog_result* result)
{
    if(!j || !result || InterlockedCompareExchange(&j->done,2,1)!=1)return 0;
    *result=j->result;memset(&j->result,0,sizeof(j->result));return 1;
}
void xideCatalogJobCancel(xide_catalog_job* j)
{if(j){InterlockedExchange(&j->cancelled,1);release(j);}}
