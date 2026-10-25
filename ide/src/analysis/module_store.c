#include "analysis/module_store.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <string.h>
typedef struct module_record {
    xavlnode tree;xatomic32 references;uint64_t id;
    xls_module_snapshot* versions[2];size_t bytes;
    char path[];
} module_record;
struct xls_module_snapshot {
    xatomic32 references;module_record* module;xls_syntax* syntax;
    uint64_t revision;size_t bytes;int resident;
    struct xls_module_snapshot *previous,*next;
};
struct xls_module_store {
    xatomic32 references;xmutex lock;xavl modules;xls_module_limits limits;
    xls_module_stats stats;xls_module_snapshot *first,*last;
    uint64_t nextModule,nextRevision;
};
static _Thread_local xls_module_scope* active;
void xlsModuleScopeBegin(xls_module_scope* scope,xls_module_store* store)
{*scope=(xls_module_scope){active,store};active=scope;}
void xlsModuleScopeEnd(xls_module_scope* scope){if(active==scope)active=scope->previous;}
xls_module_store* xlsModuleScopeCurrent(void){return active ? active->store : NULL;}
static void increment(uint64_t* value){if(*value<INT64_MAX)++*value;}
static int compare(const void* key,const xavlnode* node,void* user)
{(void)user;return strcmp(key,((const module_record*)node)->path);}
static int validPath(const char* path,size_t* bytes)
{size_t n=path ? strlen(path) : 0;if(!n || n>262144 || !xrtUtf8Valid((xstrview){path,n},NULL))return 0;*bytes=n+1;return 1;}
static module_record* recordRetain(module_record* record)
{if(record)xrtAtomic32FetchAdd(&record->references,1,XMEMORY_RELAXED);return record;}
static void recordRelease(module_record* record)
{if(record && xrtAtomic32FetchSub(&record->references,1,XMEMORY_ACQ_REL)==1)xrtFree(record);}
xls_module_snapshot* xlsModuleSnapshotRetain(xls_module_snapshot* snapshot)
{if(snapshot)xrtAtomic32FetchAdd(&snapshot->references,1,XMEMORY_RELAXED);return snapshot;}
void xlsModuleSnapshotRelease(xls_module_snapshot* snapshot)
{if(snapshot && xrtAtomic32FetchSub(&snapshot->references,1,XMEMORY_ACQ_REL)==1){xlsSyntaxRelease(snapshot->syntax);recordRelease(snapshot->module);xrtFree(snapshot);}}
uint64_t xlsModuleSnapshotIdentity(const xls_module_snapshot* snapshot){return snapshot ? snapshot->module->id : 0;}
uint64_t xlsModuleSnapshotRevision(const xls_module_snapshot* snapshot){return snapshot ? snapshot->revision : 0;}
const char* xlsModuleSnapshotPath(const xls_module_snapshot* snapshot){return snapshot ? snapshot->module->path : NULL;}
const xls_syntax* xlsModuleSnapshotSyntax(const xls_module_snapshot* snapshot){return snapshot ? snapshot->syntax : NULL;}
static void unlinkVersion(xls_module_store* s,xls_module_snapshot* v)
{
    if(v->previous)v->previous->next=v->next;else s->first=v->next;
    if(v->next)v->next->previous=v->previous;else s->last=v->previous;
    v->previous=v->next=NULL;
}
static void linkVersion(xls_module_store* s,xls_module_snapshot* v)
{v->previous=NULL;v->next=s->first;if(s->first)s->first->previous=v;else s->last=v;s->first=v;}
static void retire(xls_module_store* s,xls_module_snapshot* v,xls_module_snapshot** garbage)
{
    unlinkVersion(s,v);module_record* m=v->module;
    if(m->versions[0]==v){m->versions[0]=m->versions[1];m->versions[1]=NULL;}else if(m->versions[1]==v)m->versions[1]=NULL;
    v->resident=0;--s->stats.versions;s->stats.bytes-=v->bytes;increment(&s->stats.evictions);
    v->next=*garbage;*garbage=v;
}
static void releaseGarbage(xls_module_snapshot* garbage)
{while(garbage){xls_module_snapshot* next=garbage->next;garbage->next=NULL;xlsModuleSnapshotRelease(garbage);garbage=next;}}
static module_record* ensure(xls_module_store* s,const char* path)
{
    size_t length;if(!s || !validPath(path,&length))return NULL;
    xrtMutexLock(&s->lock);module_record* found=(module_record*)xrtAVLFind(&s->modules,path,compare,NULL);recordRetain(found);xrtMutexUnlock(&s->lock);if(found)return found;
    module_record* fresh=xrtCalloc(1,sizeof(*fresh)+length);if(!fresh)return NULL;
    memcpy(fresh->path,path,length);fresh->bytes=sizeof(*fresh)+length;xrtAtomic32Init(&fresh->references,1);xls_module_snapshot* garbage=NULL;
    xrtMutexLock(&s->lock);found=(module_record*)xrtAVLFind(&s->modules,path,compare,NULL);
    if(found)recordRetain(found);
    else if(s->stats.modules<s->limits.modules && s->nextModule<UINT64_MAX && fresh->bytes<=s->limits.bytes-s->stats.registryBytes){
        bool added=false;xavlnode* actual=xrtAVLInsert(&s->modules,&fresh->tree,fresh->path,compare,NULL,&added);
        if(added && actual==&fresh->tree){
            fresh->id=++s->nextModule;s->stats.bytes+=fresh->bytes;s->stats.registryBytes+=fresh->bytes;++s->stats.modules;
            while(s->stats.bytes>s->limits.bytes && s->last)retire(s,s->last,&garbage);
            found=recordRetain(fresh);fresh=NULL;
        }
    }
    if(!found)increment(&s->stats.declined);
    xrtMutexUnlock(&s->lock);recordRelease(fresh);releaseGarbage(garbage);return found;
}
xls_module_store* xlsModuleStoreCreate(const xls_module_limits* limits)
{
    xls_module_limits chosen=limits ? *limits : (xls_module_limits){65536,1024,64u*1024u*1024u};
    if(!chosen.modules || chosen.modules>65536 || !chosen.versions || chosen.versions>65536 || chosen.bytes<sizeof(xls_module_store) || chosen.bytes>SIZE_MAX/2)return NULL;
    xls_module_store* s=xrtCalloc(1,sizeof(*s));if(!s)return NULL;
    if(!xrtMutexInit(&s->lock)){xrtFree(s);return NULL;}
    if(!xrtAVLInit(&s->modules)){xrtMutexUnit(&s->lock);xrtFree(s);return NULL;}
    xrtAtomic32Init(&s->references,1);s->limits=chosen;s->stats.bytes=s->stats.registryBytes=sizeof(*s);s->stats.limit=chosen.bytes;return s;
}
xls_module_store* xlsModuleStoreRetain(xls_module_store* s){if(s)xrtAtomic32FetchAdd(&s->references,1,XMEMORY_RELAXED);return s;}
void xlsModuleStoreRelease(xls_module_store* s)
{
    if(!s || xrtAtomic32FetchSub(&s->references,1,XMEMORY_ACQ_REL)!=1)return;
    xls_module_snapshot* garbage=NULL;while(s->last)retire(s,s->last,&garbage);releaseGarbage(garbage);
    while(s->modules.Root){module_record* record=(module_record*)s->modules.Root;xrtAVLRemove(&s->modules,record->path,compare,NULL);recordRelease(record);}
    xrtMutexUnit(&s->lock);xrtFree(s);
}
uint64_t xlsModuleStoreIdentity(xls_module_store* s,const char* path)
{module_record* record=ensure(s,path);uint64_t id=record ? record->id : 0;recordRelease(record);return id;}
static int same(const xls_module_snapshot* v,const char* text,size_t length)
{const xls_text* body=v ? xlsSyntaxText(v->syntax) : NULL;return body && xlsTextSize(body)==length && !memcmp(xlsTextData(body),text,length);}
xls_module_snapshot* xlsModuleStoreAcquire(xls_module_store* s,const char* path,const char* text,size_t length,int* exact)
{
    if(exact)*exact=0;
    if(!s || !text || !exact)return NULL;
    module_record* record=ensure(s,path);if(!record)return NULL;
    xrtMutexLock(&s->lock);increment(&s->stats.lookups);xls_module_snapshot* a=xlsModuleSnapshotRetain(record->versions[0]);xls_module_snapshot* b=xlsModuleSnapshotRetain(record->versions[1]);xrtMutexUnlock(&s->lock);recordRelease(record);
    xls_module_snapshot* match=same(a,text,length) ? a : same(b,text,length) ? b : NULL;
    xrtMutexLock(&s->lock);
    if(match){increment(&s->stats.hits);if(match->resident){unlinkVersion(s,match);linkVersion(s,match);}*exact=1;}else increment(&s->stats.misses);
    xrtMutexUnlock(&s->lock);
    if(match && match==b){xlsModuleSnapshotRelease(a);return b;}xlsModuleSnapshotRelease(b);return a;
}
int xlsModuleStorePublish(xls_module_store* s,const char* path,xls_syntax* syntax)
{
    if(!s || !syntax || !path || strcmp(path,xlsSyntaxPath(syntax)) || !xlsSyntaxAccepted(syntax) || xlsSyntaxIsCursor(syntax))return 0;
    size_t bytes=0;if(s->limits.bytes<=sizeof(xls_module_snapshot) || !xlsSyntaxCacheBytes(syntax,s->limits.bytes-sizeof(xls_module_snapshot),&bytes))return 0;
    module_record* record=ensure(s,path);if(!record)return 0;
    xrtMutexLock(&s->lock);xls_module_snapshot* a=xlsModuleSnapshotRetain(record->versions[0]);xls_module_snapshot* b=xlsModuleSnapshotRetain(record->versions[1]);xrtMutexUnlock(&s->lock);
    const xls_text* text=xlsSyntaxText(syntax);int duplicate=same(a,xlsTextData(text),xlsTextSize(text)) || same(b,xlsTextData(text),xlsTextSize(text));
    xlsModuleSnapshotRelease(a);xlsModuleSnapshotRelease(b);
    if(duplicate){xrtMutexLock(&s->lock);increment(&s->stats.deduplicated);xrtMutexUnlock(&s->lock);recordRelease(record);return 1;}
    xls_module_snapshot* fresh=xrtCalloc(1,sizeof(*fresh));if(!fresh){recordRelease(record);return 0;}
    xrtAtomic32Init(&fresh->references,1);fresh->module=record;fresh->syntax=xlsSyntaxRetain(syntax);fresh->bytes=sizeof(*fresh)+bytes;int okay=0;xls_module_snapshot* garbage=NULL;
    xrtMutexLock(&s->lock);
    if(s->nextRevision<UINT64_MAX && fresh->bytes<=s->limits.bytes-s->stats.registryBytes){
        if(record->versions[1])retire(s,record->versions[1],&garbage);
        while(s->last && (s->stats.versions>=s->limits.versions || fresh->bytes>s->limits.bytes-s->stats.bytes))retire(s,s->last,&garbage);
        fresh->revision=++s->nextRevision;fresh->resident=1;record->versions[1]=record->versions[0];record->versions[0]=fresh;linkVersion(s,fresh);
        ++s->stats.versions;s->stats.bytes+=fresh->bytes;increment(&s->stats.published);okay=1;
    }else increment(&s->stats.declined);
    xrtMutexUnlock(&s->lock);releaseGarbage(garbage);if(!okay)xlsModuleSnapshotRelease(fresh);return okay;
}
void xlsModuleStoreStats(xls_module_store* s,xls_module_stats* stats)
{if(stats){*stats=(xls_module_stats){0};if(s){xrtMutexLock(&s->lock);*stats=s->stats;xrtMutexUnlock(&s->lock);}}}
