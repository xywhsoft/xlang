#include "analysis/input_topology.h"
#include "src/compile/xl6_source_overlay.h"
#include <xrt.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define TOPOLOGY_BYTES (8u*1024u*1024u)
#define TOPOLOGY_PATHS 65536u
typedef struct topology_key {void* data;size_t bytes;} topology_key;
struct xls_input_topology {xarray keys;size_t bytes;};
typedef struct topology_pending {xmap paths;size_t bytes,limit;} topology_pending;
static _Thread_local xls_input_topology_scope* active;
static _Thread_local int comparisonFailed;
static int compareKey(const void* a,const void* b)
{
    const topology_key *left=a,*right=b;
#ifdef _WIN32
    /* Same prepared UTF-16/ordinal comparison as xl6SourceOverlayPathEquals;
     * no locale case folding,physical aliases or extra path normalization. */
    int order=CompareStringOrdinal(left->data,-1,right->data,-1,TRUE);
    if(!order){comparisonFailed=1;return 0;}return order-CSTR_EQUAL;
#else
    return strcmp(left->data,right->data);
#endif
}
static topology_key prepare(const char* path,int absolute)
{
    topology_key key={0};size_t length=path ? strlen(path) : 0;
    if(!length || length>262144 || !xrtUtf8Valid((xstrview){path,length},NULL))return key;
    char* owned=absolute ? NULL : xrtPathAbs(path);const char* text=absolute ? path : owned;
    if(!text)return key;
#ifdef _WIN32
    size_t count=0;key.data=xrtUtf8To16(text,&count);if(key.data)key.bytes=(count+1)*sizeof(uint16);
#else
    key.data=xrtStrDup(text);if(key.data)key.bytes=strlen(text)+1;
#endif
    xrtFree(owned);return key;
}
void xlsInputTopologyBegin(xls_input_topology_scope* scope,size_t bytes)
{
    if(!scope)return;
    *scope=(xls_input_topology_scope){.previous=active};active=scope;
    if(!bytes)return;
    if(bytes>TOPOLOGY_BYTES)bytes=TOPOLOGY_BYTES;
    topology_pending* pending=xrtCalloc(1,sizeof(*pending));
    if(pending && xrtMapInit(&pending->paths,sizeof(unsigned char))){pending->limit=bytes;scope->state=pending;}
    else{if(pending)xrtMapUnit(&pending->paths);xrtFree(pending);scope->failed=1;}
}
void xlsInputTopologyBind(const struct XL6_SourceOverlay* overlay,size_t count)
{
    if(!active || !active->state)return;
    if(active->bound || !overlay || !count || count>64){active->failed=1;return;}
    active->overlay=overlay;active->bound=1;
}
void xlsInputTopologyFailure(const struct XL6_SourceOverlay* overlay)
{if(active && active->bound && (!overlay || active->overlay==overlay))active->failed=1;}
void xlsInputTopologyReportFailure(const struct XL6_SourceReport* report)
{if(report && active && active->bound && active->overlay && active->overlay->report==report)active->failed=1;}
void xlsInputTopologyLookup(const struct XL6_SourceOverlay* overlay,const char* absolute)
{
    if(!active || !active->state || active->failed || !active->bound || active->overlay!=overlay)return;
    topology_pending* p=active->state;size_t length=absolute ? strlen(absolute) : 0;
    if(!length || length>262144){active->failed=1;return;}
    xbytesview key={(const unsigned char*)absolute,length+1};
    if(xrtMapHas(&p->paths,key))return;
    size_t bytes=length+1+sizeof(topology_key)+sizeof(unsigned char);unsigned char one=1;
    if(bytes>p->limit-p->bytes || xrtMapCount(&p->paths)>=TOPOLOGY_PATHS || !xrtMapSet(&p->paths,key,&one))active->failed=1;
    else p->bytes+=bytes;
}
void xlsInputTopologyDestroy(xls_input_topology* topology)
{
    if(topology){for(size_t i=0;i<topology->keys.Count;++i)xrtFree(((topology_key*)xrtArrayGet(&topology->keys,i))->data);xrtArrayUnit(&topology->keys);xrtFree(topology);}
}
xls_input_topology* xlsInputTopologyClone(const xls_input_topology* source,size_t limit,int (*cancelled)(void*),void* user)
{
    if(limit>TOPOLOGY_BYTES)limit=TOPOLOGY_BYTES;
    if(!source || limit<sizeof(xls_input_topology) || source->keys.Count>TOPOLOGY_PATHS || (cancelled && cancelled(user)))return NULL;
    xls_input_topology* copy=xrtCalloc(1,sizeof(*copy));if(!copy)return NULL;
    if(!xrtArrayInit(&copy->keys,sizeof(topology_key)) || !xrtArrayReserve(&copy->keys,source->keys.Count) ||
       copy->keys.Capacity>(limit-sizeof(*copy))/sizeof(topology_key))goto failed;
    copy->bytes=sizeof(*copy)+copy->keys.Capacity*sizeof(topology_key);
    for(size_t i=0;i<source->keys.Count;++i){
        if(cancelled && cancelled(user))goto failed;
        const topology_key* from=xrtArrayConstGet(&source->keys,i);
        if(!from || !from->data || !from->bytes || from->bytes>limit-copy->bytes)goto failed;
        topology_key key={xrtMalloc(from->bytes),from->bytes};if(!key.data)goto failed;
        memcpy(key.data,from->data,from->bytes);
        if(!xrtArrayPush(&copy->keys,&key)){xrtFree(key.data);goto failed;}
        copy->bytes+=key.bytes;
    }
    if(cancelled && cancelled(user))goto failed;
    return copy;
failed:xlsInputTopologyDestroy(copy);return NULL;
}
xls_input_topology* xlsInputTopologyEnd(xls_input_topology_scope* scope)
{
    if(!scope || active!=scope)return NULL;
    active=scope->previous;topology_pending* p=scope->state;scope->state=NULL;xls_input_topology* result=NULL;
    if(p && scope->bound && !scope->failed){
        result=xrtCalloc(1,sizeof(*result));
        int okay=result && p->limit>=sizeof(*result) && xrtArrayInit(&result->keys,sizeof(topology_key)) && xrtArrayReserve(&result->keys,xrtMapCount(&p->paths));
        size_t fixed=sizeof(*result);
        if(okay){okay=result->keys.Capacity<=(p->limit-fixed)/sizeof(topology_key);if(okay)result->bytes=fixed+result->keys.Capacity*sizeof(topology_key);}
        xmapiter iter;int begun=okay && xrtMapIterBegin(&p->paths,&iter);okay=okay && begun;xbytesview path;
        while(okay && xrtMapIterNext(&iter,&path)){
            topology_key key=prepare((const char*)path.Data,1);
            if(!key.data || key.bytes>p->limit-result->bytes || !xrtArrayPush(&result->keys,&key)){xrtFree(key.data);okay=0;break;}
            result->bytes+=key.bytes;
        }
        if(begun)xrtMapIterEnd(&iter);
        if(okay){comparisonFailed=0;if(result->keys.Count>1)qsort(result->keys.Data,result->keys.Count,sizeof(topology_key),compareKey);okay=!comparisonFailed;}
        if(!okay){xlsInputTopologyDestroy(result);result=NULL;}
    }
    if(p){xrtMapUnit(&p->paths);xrtFree(p);}return result;
}
xls_input_topology_info xlsInputTopologyInfo(const xls_input_topology* topology)
{return topology ? (xls_input_topology_info){topology->keys.Count,topology->bytes} : (xls_input_topology_info){0};}
static int absent(const xls_input_topology* topology,topology_key key)
{
    size_t lo=0,hi=topology->keys.Count;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;int order=compareKey(&key,xrtArrayConstGet(&topology->keys,mid));if(!order)return 0;if(order<0)hi=mid;else lo=mid+1;}
    return 1;
}
int xlsInputTopologyAdmits(const xls_input_topology* topology,const char* const* changed,size_t changedCount,const char* const* current,size_t count)
{
    if(!topology || changedCount>128 || (changedCount && !changed) || !current || !count || count>64)return 0;
    topology_key keys[64]={{0}};int okay=1;comparisonFailed=0;
    for(size_t i=0;i<count;++i){keys[i]=prepare(current[i],0);if(!keys[i].data){okay=0;break;}}
    if(okay){qsort(keys,count,sizeof(*keys),compareKey);for(size_t i=1;i<count;++i)if(!compareKey(&keys[i-1],&keys[i])){okay=0;break;}}
    for(size_t i=0;okay && i<changedCount;++i){topology_key key=prepare(changed[i],0);okay=key.data && absent(topology,key);xrtFree(key.data);}
    for(size_t i=0;i<count;++i)xrtFree(keys[i].data);
    return okay && !comparisonFailed;
}
int xlsInputTopologyOverlayCompatible(const xls_input_topology* topology,const XL6_SourceOverlay* previous,const XL6_SourceOverlay* current)
{
    if(!previous || !current || previous->count>64 || !current->count || current->count>64)return 0;
    const char *changed[128],*paths[64];size_t count=0;
    for(size_t i=0;i<previous->count;++i){
        size_t j=0;for(;j<current->count;++j)if(!strcmp(previous->items[i].path,current->items[j].path))break;
        if(j==current->count)changed[count++]=previous->items[i].path;
    }
    for(size_t i=0;i<current->count;++i){
        paths[i]=current->items[i].path;size_t j=0;
        for(;j<previous->count;++j)if(!strcmp(paths[i],previous->items[j].path))break;
        if(j==previous->count)changed[count++]=paths[i];
    }
    return !count || xlsInputTopologyAdmits(topology,changed,count,paths,current->count);
}
