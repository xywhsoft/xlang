#include "protocol/document_sync.h"
#include "protocol/input_state.h"
#include "analysis/text.h"
#include <string.h>
#include <limits.h>

#define DOC_LIMIT 64u
#define TEXT_LIMIT (8u*1024u*1024u)
#define TOTAL_LIMIT (32u*1024u*1024u)
typedef struct sync_entry {
    xatomic32 references;
    char* uri;
    uint64_t identity,revision;
    int version;
    unsigned char uriHash[32];
    xls_text* text;
} sync_entry;
typedef struct sync_message { const char* method; xvalue* params; size_t textBytes; } sync_message;
struct xls_sync { xarray entries; int kind,failed,monotonic,nextVersion; xls_sync_stats stats; };

static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static int put(xvalue* object,const char* name,xvalue* value)
{
    if(!value) return 0;
    int okay=xrtValueObjectSet(object,sv(name),value);
    xrtValueRelease(value); return okay;
}
static sync_entry* retain(sync_entry* entry) { if(entry) xrtAtomic32FetchAdd(&entry->references,1,XMEMORY_RELAXED); return entry; }
static void release(sync_entry* entry)
{
    if(entry && xrtAtomic32FetchSub(&entry->references,1,XMEMORY_ACQ_REL)==1) { xlsTextRelease(entry->text); xrtFree(entry->uri); xrtFree(entry); }
}
static void freeEntries(xarray* entries)
{
    for(size_t i=0;i<entries->Count;++i) release(*(sync_entry**)xrtArrayGet(entries,i));
    xrtArrayUnit(entries);
}
static sync_entry* find(const xarray* entries,const char* uri)
{
    for(size_t i=0;i<entries->Count;++i) {
        sync_entry* entry=*(sync_entry*const*)xrtArrayConstGet(entries,i);
        if(!strcmp(entry->uri,uri)) return entry;
    }
    return NULL;
}
static sync_entry* createEntry(const xls_sync_document* document,sync_entry* previous,int* clock)
{
    sync_entry* entry=xrtMalloc(sizeof(*entry));
    if(!entry) return NULL;
    memset(entry,0,sizeof(*entry)); xrtAtomic32Init(&entry->references,1);
    size_t size=strlen(document->uri)+1;
    entry->uri=xrtMalloc(size);
    if(!entry->uri) { release(entry); return NULL; }
    memcpy(entry->uri,document->uri,size);
    if(previous) memcpy(entry->uriHash,previous->uriHash,32);
    else if(!xlsInputStateUri(entry->uri,entry->uriHash)) { release(entry); return NULL; }
    entry->identity=document->identity; entry->revision=document->revision;
    int same=previous && previous->identity==document->identity;
    if(same && xlsTextSize(previous->text)==document->length &&
        !memcmp(xlsTextData(previous->text),document->text,document->length)) {
        entry->text=xlsTextRetain(previous->text); entry->version=previous->version;
    } else {
        if((clock && *clock==INT_MAX) || (!clock && same && previous->version==INT_MAX)) { release(entry); return NULL; }
        entry->version=clock ? ++*clock : same ? previous->version+1 : 1;
        entry->text=xlsTextCreate(document->text,document->length,TEXT_LIMIT,NULL);
    }
    if(!entry->text) { release(entry); return NULL; }
    return entry;
}

xls_sync* xlsSyncCreate(int changeKind)
{
    if(changeKind!=1 && changeKind!=2) return NULL;
    xls_sync* sync=xrtMalloc(sizeof(*sync));
    if(!sync) return NULL;
    memset(sync,0,sizeof(*sync)); sync->kind=changeKind;
    if(!xrtArrayInit(&sync->entries,sizeof(sync_entry*))) { xrtFree(sync); return NULL; }
    return sync;
}
xls_sync* xlsSyncCreateMonotonic(int changeKind)
{xls_sync* sync=xlsSyncCreate(changeKind);if(sync) sync->monotonic=1;return sync;}
int xlsSyncBindingAt(const xls_sync* sync,size_t index,xls_sync_binding* binding)
{
    if(binding) *binding=(xls_sync_binding){0};
    if(!sync || sync->failed || !binding || index>=sync->entries.Count) return 0;
    const sync_entry* entry=*(sync_entry*const*)xrtArrayConstGet(&sync->entries,index);
    *binding=(xls_sync_binding){entry->uri,entry->identity,entry->revision,entry->version};return 1;
}
void xlsSyncDestroy(xls_sync* sync)
{ if(sync) { freeEntries(&sync->entries); xrtFree(sync); } }
xls_sync* xlsSyncClone(const xls_sync* sync)
{
    if(!sync || sync->failed) return NULL;
    xls_sync* result=xlsSyncCreate(sync->kind);if(!result) return NULL;
    for(size_t i=0;i<sync->entries.Count;++i) {
        sync_entry* entry=retain(*(sync_entry*const*)xrtArrayConstGet(&sync->entries,i));
        if(!xrtArrayPush(&result->entries,&entry)) { release(entry);xlsSyncDestroy(result);return NULL; }
    }
    result->stats=sync->stats;result->monotonic=sync->monotonic;result->nextVersion=sync->nextVersion;return result;
}
int xlsSyncVersion(const xls_sync* sync,const char* uri)
{
    sync_entry* entry=sync && !sync->failed && uri ? find(&sync->entries,uri) : NULL;
    return entry ? entry->version : 0;
}
xls_text* xlsSyncRetainText(const xls_sync* sync,const char* uri,int version)
{
    sync_entry* entry=sync && !sync->failed && uri && version>0 ? find(&sync->entries,uri) : NULL;
    return entry && entry->version==version ? xlsTextRetain(entry->text) : NULL;
}
void xlsSyncStats(const xls_sync* sync,xls_sync_stats* stats)
{ if(stats) *stats=sync ? sync->stats : (xls_sync_stats){0}; }
int xlsSyncInputState(const xls_sync* sync,char digest[65],size_t* documents)
{
    if(digest) digest[0]=0;
    if(documents) *documents=0;
    if(!sync || sync->failed || !digest || !documents || sync->entries.Count>DOC_LIMIT) return 0;
    xls_input_state_item items[DOC_LIMIT];
    for(size_t i=0;i<sync->entries.Count;++i) {
        const sync_entry* entry=*(sync_entry*const*)xrtArrayConstGet(&sync->entries,i);
        memcpy(items[i].uri,entry->uriHash,32); items[i].version=entry->version;
    }
    if(!xlsInputStateDigest(items,sync->entries.Count,digest)) return 0;
    *documents=sync->entries.Count; return 1;
}

static xvalue* position(xls_position at)
{
    xvalue* result=xrtValueObject();
    if(!result || !put(result,"line",xrtValueInt((int64)at.line)) ||
        !put(result,"character",xrtValueInt((int64)at.character))) { xrtValueRelease(result); return NULL; }
    return result;
}
static int plan(xarray* messages,const char* method,sync_entry* entry,sync_entry* previous,int kind)
{
    xvalue *params=xrtValueObject(),*document=xrtValueObject(),*changes=NULL,*change=NULL,*range=NULL;
    int okay=params && document && put(document,"uri",xrtValueString(sv(entry->uri)));
    size_t bytes=0;
    if(okay && strcmp(method,"textDocument/didClose")) okay=put(document,"version",xrtValueInt(entry->version));
    if(okay && !strcmp(method,"textDocument/didOpen")) {
        bytes=xlsTextSize(entry->text);
        okay=put(document,"languageId",xrtValueString(sv("xlang"))) &&
            put(document,"text",xrtValueString((xstrview){xlsTextData(entry->text),bytes}));
    } else if(okay && !strcmp(method,"textDocument/didChange")) {
        changes=xrtValueArray(); change=xrtValueObject();
        xls_text_edit delta={0};
        if(kind==2) {
            range=xrtValueObject();
            okay=range && xlsTextDiff(previous->text,entry->text,&delta)==1 &&
                put(range,"start",position(delta.start)) && put(range,"end",position(delta.end)) &&
                xrtValueObjectSet(change,sv("range"),range);
        } else { delta.text=xlsTextData(entry->text); delta.length=xlsTextSize(entry->text); }
        bytes=delta.length;
        okay=okay && changes && change && put(change,"text",xrtValueString((xstrview){delta.text,delta.length})) &&
            xrtValueArrayAppend(changes,change) && xrtValueObjectSet(params,sv("contentChanges"),changes);
    }
    okay=okay && xrtValueObjectSet(params,sv("textDocument"),document);
    xrtValueRelease(document); xrtValueRelease(changes); xrtValueRelease(change); xrtValueRelease(range);
    sync_message message={method,params,bytes};
    if(okay && xrtArrayPush(messages,&message)) return 1;
    xrtValueRelease(params); return 0;
}

int xlsSyncUpdate(xls_sync* sync,const xls_sync_document* documents,size_t count,
                  xls_sync_emit emit,void* user)
{
    if(!sync || sync->failed || !emit || count>DOC_LIMIT || (count && !documents)) return 0;
    xarray next={0},messages={0}; size_t total=0; int okay=0,nextVersion=sync->nextVersion;
    if(!xrtArrayInit(&next,sizeof(sync_entry*)) || !xrtArrayInit(&messages,sizeof(sync_message))) goto finished;
    for(size_t i=0;i<count;++i) {
        const xls_sync_document* document=&documents[i];
        if(!document->uri || !*document->uri || !document->text || document->length>TEXT_LIMIT ||
            document->length>TOTAL_LIMIT-total || strlen(document->uri)>262144 ||
            !xrtUtf8Valid(sv(document->uri),NULL) || find(&next,document->uri)) goto finished;
        total+=document->length;
        sync_entry* old=find(&sync->entries,document->uri);
        sync_entry* entry=old && old->identity==document->identity && old->revision==document->revision ?
            retain(old) : createEntry(document,old,sync->monotonic ? &nextVersion : NULL);
        if(!entry) goto finished;
        if(xlsTextSize(entry->text)!=document->length || !xrtArrayPush(&next,&entry)) { release(entry); goto finished; }
    }
    for(size_t i=0;i<sync->entries.Count;++i) {
        sync_entry* old=*(sync_entry**)xrtArrayGet(&sync->entries,i),*entry=find(&next,old->uri);
        if((!entry || old->identity!=entry->identity) && !plan(&messages,"textDocument/didClose",old,NULL,sync->kind)) goto finished;
    }
    for(size_t i=0;i<next.Count;++i) {
        sync_entry* entry=*(sync_entry**)xrtArrayGet(&next,i),*old=find(&sync->entries,entry->uri);
        if(!old || entry->identity!=old->identity) {
            if(!plan(&messages,"textDocument/didOpen",entry,NULL,sync->kind)) goto finished;
        } else if(entry->text!=old->text && !plan(&messages,"textDocument/didChange",entry,old,sync->kind)) goto finished;
    }
    for(size_t i=0;i<messages.Count;++i) {
        sync_message* message=xrtArrayGet(&messages,i);
        if(!emit(user,message->method,message->params)) { sync->failed=1; goto finished; }
        if(!strcmp(message->method,"textDocument/didOpen")) ++sync->stats.opens;
        else if(!strcmp(message->method,"textDocument/didChange")) ++sync->stats.changes;
        else ++sync->stats.closes;
        sync->stats.textBytes+=message->textBytes;
    }
    freeEntries(&sync->entries); sync->entries=next;sync->nextVersion=nextVersion;memset(&next,0,sizeof(next)); okay=1;
finished:
    freeEntries(&next);
    for(size_t i=0;i<messages.Count;++i) xrtValueRelease(((sync_message*)xrtArrayGet(&messages,i))->params);
    xrtArrayUnit(&messages); return okay;
}
