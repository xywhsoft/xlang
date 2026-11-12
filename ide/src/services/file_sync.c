#include "ui/template_view.h"
#include "ui/references_view.h"
#include "ui/file_changes_view.h"
#include "services/file_sync.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/reading_state.h"
#include "ui/font_view.h"
#include "ui/configuration_view.h"
#include "ui/recovery_view.h"
#include "ui/language_view.h"
#include "ui/completion_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LIMIT (16u*1024u*1024u)
#define PENDING_BYTES (128u*1024u*1024u)
#define PENDING_COUNT 4096u
enum { DOCUMENT=1,WORKSPACE=2,PROJECT=4,FILE_ENTRY=8 };
typedef struct subscription { xide_watch_target target; char* path; uint64_t documentId,savedAt,documentEpoch; int roles,seen; xide_watch_event last; } subscription;
struct xide_file_sync {
    xide_file_watch* watch; subscription* targets; size_t count,capacity;
    uint64_t documentEpoch,epoch,nextToken,fingerprint,revision,dropped,holdToken; xide_file_change* changes; size_t changeCount,changeCapacity,bytes;
    int refreshTree,retryDropped; uint64_t nextRescan,languageDropped,languageStoreDropped; char status[192];
};
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static uint64_t hash(uint64_t h,const void* p,size_t n) { const unsigned char* s=p; for(size_t i=0;i<n;++i) { h^=s[i]; h*=UINT64_C(1099511628211); } return h; }
static uint64_t textHash(uint64_t h,const char* s) { return hash(h,s ? s : "",s ? strlen(s)+1 : 1); }
static uint64_t fingerprint(xide_app* app)
{
    uint64_t h=UINT64_C(14695981039346656037),epoch=xideDocumentsSourceEpoch(&app->documents);h=hash(h,&epoch,sizeof(epoch)); xide_workspace* w=&app->workspaceModel; h=textHash(h,w->manifest);
    for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->path) { h=hash(h,&d->id,sizeof(d->id)); h=textHash(h,d->path); } }
    for(size_t i=0;i<w->projectCount;++i) { xide_project* p=&w->projects[i]; h=textHash(h,p->id); h=textHash(h,p->manifest);h=textHash(h,p->entry); for(size_t j=0;j<p->files.count;++j) h=textHash(h,p->files.items[j].path); }
    for(size_t i=0;i<w->files.count;++i) h=textHash(h,w->files.items[i].path);
    return h;
}
static void clearTargets(xide_file_sync* s) { for(size_t i=0;i<s->count;++i) free(s->targets[i].path); free(s->targets); s->targets=NULL; s->count=s->capacity=0; }
static void removeChange(xide_file_sync* s,size_t i)
{ s->bytes-=s->changes[i].snapshot.length; xideFileWatchEventClear(&s->changes[i].snapshot); --s->changeCount; memmove(s->changes+i,s->changes+i+1,(s->changeCount-i)*sizeof(*s->changes)); ++s->revision; }
static int addTarget(xide_file_sync* s,const char* path,int roles,uint64_t documentId)
{
    if(!path || !*path) return 1;
    for(size_t i=0;i<s->count;++i) if(!_stricmp(s->targets[i].path,path)) { s->targets[i].roles|=roles; if(documentId) s->targets[i].documentId=documentId; if(roles&(DOCUMENT|WORKSPACE|PROJECT)) s->targets[i].target.limit=LIMIT; return 1; }
    if(s->count==s->capacity) { size_t cap=s->capacity ? s->capacity*2 : 64; subscription* p=realloc(s->targets,cap*sizeof(*p)); if(!p) return 0; s->targets=p; s->capacity=cap; }
    char* p=copy(path); if(!p) return 0;
    s->targets[s->count++]=(subscription){.target={++s->nextToken,p,roles&(DOCUMENT|WORKSPACE|PROJECT) ? LIMIT : 0},.path=p,.documentId=documentId,.documentEpoch=s->documentEpoch,.roles=roles}; return 1;
}
static subscription* targetByToken(xide_file_sync* s,uint64_t token)
{ for(size_t i=0;i<s->count;++i) if(s->targets[i].target.token==token) return &s->targets[i]; return NULL; }
static xide_document* documentById(xide_app* app,uint64_t id)
{ for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->id==id) return d; } return NULL; }
static xvalue* json(const unsigned char* raw,size_t length,xide_model_error* e)
{
    xide_text_file f={0}; if(!xideTextFileDecode(raw,length,&f,e)) return NULL;
    xvalue* root=xrtJsonParse((xstrview){f.text,strlen(f.text)}); xideTextFileClear(&f);
    if(!root || !xrtValueIs(root,XVALUE_OBJECT)) { xrtValueRelease(root); fail(e,XIDE_MODEL_INVALID,"Manifest is not a valid UTF-8 JSON object."); return NULL; } return root;
}
static void refreshTree(xide_app* app,const xvalue* reading)
{
    xideWorkspaceViewRefresh(app); xideReadingRestore(app,reading,0); xideWorkbenchLayout(app); xuiLayout(app->context); xideReadingRestore(app,reading,1);
}
static int coordinate(xide_app* app,const char* path,const xvalue* root,int discard,int commit,xide_model_error* e)
{
    xide_workspace* w=&app->workspaceModel; xide_workspace next={0}; xide_project candidate={0}; xide_project* old=NULL;
    int isWorkspace=w->manifest && xidePathEqual(w->manifest,path);
    if(!isWorkspace) for(size_t i=0;i<w->projectCount;++i) if(xidePathEqual(w->projects[i].manifest,path)) { old=&w->projects[i]; break; }
    if(!isWorkspace && !old) return 1;
    if(xrtValueEqual(root,isWorkspace ? w->source : old->source)) return 1;
    if(!discard && ((isWorkspace && w->dirty) || (old && old->dirty) || xideWorkspaceViewHasDraft(app))) return fail(e,XIDE_MODEL_CONFLICT,"Manifest or project property inputs have unsaved changes; keep the local version or explicitly reload.");
    if(isWorkspace) {
        if(!xideWorkspaceDecode(&next,path,root,e)) return 0;
        for(size_t i=0;i<w->projectCount;++i) {
            xide_project* prior=&w->projects[i]; xide_project* ref=xideWorkspaceFindProject(&next,prior->id);
            if(!discard && prior->dirty && (!ref || !xidePathEqual(ref->manifest,prior->manifest))) { xideWorkspaceClear(&next); return fail(e,XIDE_MODEL_CONFLICT,"External workspace removes or rebinds a project with unsaved properties."); }
        }
    } else {
        if(!xideProjectDecode(&candidate,path,root,e)) return 0;
        candidate.id=copy(old->id); candidate.referenceSource=xrtValueClone(old->referenceSource);
        if(!candidate.id || !candidate.referenceSource) { xideProjectClear(&candidate); return fail(e,XIDE_MODEL_MEMORY,"Cannot preserve project reference identity."); }
        if(old->activeConfiguration && xideProjectFindConfiguration(&candidate,old->activeConfiguration)) { char* active=copy(old->activeConfiguration); if(!active) { xideProjectClear(&candidate); return fail(e,XIDE_MODEL_MEMORY,"Cannot preserve active project configuration."); } free(candidate.activeConfiguration); candidate.activeConfiguration=active; }
    }
    if(!commit) { xideWorkspaceClear(&next); xideProjectClear(&candidate); return 1; }
    xvalue* reading=xideReadingCapture(app,NULL);
    if(!reading) { xideWorkspaceClear(&next); xideProjectClear(&candidate); return fail(e,XIDE_MODEL_MEMORY,"Cannot preserve workspace reading positions."); }
    if(isWorkspace) {
        for(size_t i=0;i<next.projectCount;++i) {
            xide_project* ref=&next.projects[i]; xide_project* prior=NULL;
            for(size_t j=0;j<w->projectCount;++j) if(w->projects[j].id && !strcmp(w->projects[j].id,ref->id)) { prior=&w->projects[j]; break; }
            if(prior && xidePathEqual(ref->manifest,prior->manifest)) {
                xvalue* reference=ref->referenceSource; ref->referenceSource=NULL; xideProjectClear(ref); *ref=*prior; memset(prior,0,sizeof(*prior)); xrtValueRelease(ref->referenceSource); ref->referenceSource=reference;
            }
        }
        xideWorkspaceClear(w); *w=next;
    } else { xideProjectClear(old); *old=candidate; }
    refreshTree(app,reading); xrtValueRelease(reading); return 1;
}
static int isManifest(xide_app* app,const char* path)
{
    if(app->workspaceModel.manifest && xidePathEqual(app->workspaceModel.manifest,path)) return 1;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(xidePathEqual(app->workspaceModel.projects[i].manifest,path)) return 1;
    return 0;
}
static int apply(xide_app* app,const subscription* t,const xide_watch_event* e,int discard,xide_model_error* error)
{
    if(xideDocumentsSourcesHeld(&app->documents) || t->documentEpoch!=xideDocumentsSourceEpoch(&app->documents))return fail(error,XIDE_MODEL_CONFLICT,"File snapshot predates a document source transaction.");
    xide_document* d=t->documentId ? documentById(app,t->documentId) : NULL;
    if(e->state!=XIDE_WATCH_READY) {
        const char* message=e->state==XIDE_WATCH_MISSING ? "File was removed or moved on disk; the local contents and project reference are retained." : e->state==XIDE_WATCH_TOO_LARGE ? "File exceeds the 16 MiB background read limit; the local contents are retained." : e->state==XIDE_WATCH_DIRECTORY ? "The watched file path now names a directory." : e->state==XIDE_WATCH_UNSTABLE ? "The file changed during reading; waiting for a stable disk snapshot." : "Cannot read the disk file; the local contents are retained.";
        return fail(error,XIDE_MODEL_CONFLICT,message);
    }
    int different=d && (e->length!=d->baseline.rawLength || (e->length && memcmp(e->bytes,d->baseline.raw,e->length)));
    if(different && d->dirty && !discard) return fail(error,XIDE_MODEL_CONFLICT,"Disk contents changed while this document has unsaved edits. Keep local edits, compare or explicitly reload.");
    if(isManifest(app,e->path)) { xvalue* root=json(e->bytes,e->length,error); if(!root) return 0; int okay=coordinate(app,e->path,root,discard,1,error); xrtValueRelease(root); if(!okay) return 0; }
    if(different) {
        xide_text_file loaded={0}; if(!xideTextFileDecode(e->bytes,e->length,&loaded,error)) return 0;
        int okay=xideDocumentAcceptExternal(d,&loaded,discard,t->documentEpoch,error); xideTextFileClear(&loaded); return okay;
    }
    if(d) d->externalState=0;
    return 1;
}
static uint64_t localSignature(xide_app* app,uint64_t documentId)
{
    uint64_t h=UINT64_C(14695981039346656037); xide_document* d=documentById(app,documentId);
    if(d) { uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); h=hash(h,&version,sizeof(version)); h=hash(h,&d->dirty,sizeof(d->dirty)); }
    xide_workspace* w=&app->workspaceModel; h=hash(h,&w->source,sizeof(w->source)); h=hash(h,&w->dirty,sizeof(w->dirty));
    for(size_t i=0;i<w->projectCount;++i) { xide_project* p=&w->projects[i]; h=hash(h,&p->source,sizeof(p->source)); h=hash(h,&p->dirty,sizeof(p->dirty)); }
    int draft=xideWorkspaceViewHasDraft(app); return hash(h,&draft,sizeof(draft));
}
static int sameStamp(const xide_watch_event* a,const xide_watch_event* b)
{ return a->state==b->state && a->error==b->error && a->size==b->size && a->hash==b->hash && a->writeTime==b->writeTime && a->fileId==b->fileId && a->volume==b->volume; }
static int alreadyKnown(xide_app* app,const subscription* t,const xide_watch_event* event)
{
    if(event->state!=XIDE_WATCH_READY) return 0;
    xide_document* d=documentById(app,t->documentId);
    if(d && (event->length!=d->baseline.rawLength || (event->length && memcmp(event->bytes,d->baseline.raw,event->length)))) return 0;
    if(isManifest(app,event->path)) {
        const xvalue* baseline=NULL; if(app->workspaceModel.manifest && xidePathEqual(app->workspaceModel.manifest,event->path)) baseline=app->workspaceModel.source;
        else for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(xidePathEqual(app->workspaceModel.projects[i].manifest,event->path)) { baseline=app->workspaceModel.projects[i].source; break; }
        xide_model_error error={0}; xvalue* root=json(event->bytes,event->length,&error); int same=root && xrtValueEqual(root,baseline); xrtValueRelease(root); return same;
    }
    return d!=NULL;
}
static int paused(xide_app* app) { return xideActionsHasModal(app); }
static int storeChange(xide_file_sync* s,subscription* t,xide_watch_event* e,const char* reason)
{
    size_t i=0; for(;i<s->changeCount;++i) if(s->changes[i].snapshot.token==e->token) break;
    size_t old=i<s->changeCount ? s->changes[i].snapshot.length : 0;
    if(s->bytes-old+e->length>PENDING_BYTES || (i==s->changeCount && s->changeCount==PENDING_COUNT)) { ++s->dropped; return 0; }
    if(i==s->changeCapacity) { size_t cap=s->changeCapacity ? s->changeCapacity*2 : 16; xide_file_change* p=realloc(s->changes,cap*sizeof(*p)); if(!p) { ++s->dropped; return 0; } s->changes=p; s->changeCapacity=cap; }
    if(i<s->changeCount) { xide_file_change* oldChange=&s->changes[i]; xide_watch_event* a=&oldChange->snapshot; if(oldChange->acknowledged && a->state==e->state && a->fileId==e->fileId && a->volume==e->volume && a->hash==e->hash && a->size==e->size && a->writeTime==e->writeTime) return 1; xideFileWatchEventClear(a); } else ++s->changeCount;
    s->bytes=s->bytes-old+e->length; s->changes[i]=(xide_file_change){.snapshot=*e,.documentId=t->documentId,.roles=t->roles}; memset(e,0,sizeof(*e)); snprintf(s->changes[i].reason,sizeof(s->changes[i].reason),"%s",reason); ++s->revision; return 1;
}
static void wake(void* native) { if(native) PostMessageW((HWND)native,WM_NULL,0,0); }
int xideFileSyncStart(xide_app* app,xide_model_error* e)
{
    if(app->fileSync) return 1;
    xide_file_sync* s=calloc(1,sizeof(*s)); if(!s) return fail(e,XIDE_MODEL_MEMORY,"Cannot allocate file coordination service.");
    s->watch=xideFileWatchCreate(wake,xgePlatformNativeHandle(),e); if(!s->watch) { free(s); return 0; } app->fileSync=s; return 1;
}
void xideFileSyncStop(xide_app* app)
{
    xide_file_sync* s=app->fileSync; if(!s) return;
    /* The pinned status bar borrows its text pointer. Detach service storage
       while the widget is still alive, before releasing the monitor. */
    if(app->statusBar && app->context) xuiStatusBarSetItemText(app->statusBar,0,"就绪");
    xideFileWatchDestroy(s->watch); clearTargets(s); for(size_t i=0;i<s->changeCount;++i) xideFileWatchEventClear(&s->changes[i].snapshot); free(s->changes); free(s); app->fileSync=NULL;
}
static int subscriptionPathCompare(const void* left,const void* right)
{return strcmp((*(subscription* const*)left)->path,(*(subscription* const*)right)->path);}
static int retainSubscriptionStamps(xide_file_sync* fresh,const xide_file_sync* previous)
{
    if(!previous->count || previous->documentEpoch!=fresh->documentEpoch)return 1;
    subscription** sorted=malloc(previous->count*sizeof(*sorted));if(!sorted)return 0;
    for(size_t i=0;i<previous->count;++i)sorted[i]=previous->targets+i;
    qsort(sorted,previous->count,sizeof(*sorted),subscriptionPathCompare);
    for(size_t i=0;i<fresh->count;++i){subscription* now=fresh->targets+i;subscription* probe=now;
        subscription** found=bsearch(&probe,sorted,previous->count,sizeof(*sorted),subscriptionPathCompare);
        if(found){now->seen=(*found)->seen;now->last=(*found)->last;
            /* Closing an editor changes its remaining project subscription
               from body+metadata to metadata only. The absent body hash is
               not a disk edit. A newly body-reading document establishes its
               content baseline through the ordinary apply/conflict path. */
            if(!now->target.limit){now->last.hash=0;now->last.length=0;}
            else if(!(*found)->target.limit)now->seen=0;
            if(now->documentId==(*found)->documentId)now->savedAt=(*found)->savedAt;}
    }
    free(sorted);return 1;
}
static int subscribe(xide_app* app,xide_model_error* e)
{
    xide_file_sync* s=app->fileSync; uint64_t f=fingerprint(app); if(f==s->fingerprint && s->epoch) return 1;
    xide_file_sync fresh={.nextToken=s->nextToken,.documentEpoch=xideDocumentsSourceEpoch(&app->documents)}; xide_workspace* w=&app->workspaceModel; int okay=addTarget(&fresh,w->manifest,WORKSPACE,0);
    for(size_t i=0;okay && i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->path) okay=addTarget(&fresh,d->path,DOCUMENT,d->id); }
    for(size_t i=0;okay && i<w->projectCount;++i) { xide_project* p=&w->projects[i]; okay=addTarget(&fresh,p->manifest,PROJECT,0) && addTarget(&fresh,p->entry,FILE_ENTRY,0); for(size_t j=0;okay && j<p->files.count;++j) okay=addTarget(&fresh,p->files.items[j].path,FILE_ENTRY,0); }
    for(size_t i=0;okay && i<w->files.count;++i) okay=addTarget(&fresh,w->files.items[i].path,FILE_ENTRY,0);
    if(okay && !retainSubscriptionStamps(&fresh,s)){clearTargets(&fresh);return fail(e,XIDE_MODEL_MEMORY,"Cannot preserve file watch baselines;subscriptions retained.");}
    xide_watch_target* targets=fresh.count ? calloc(fresh.count,sizeof(*targets)) : NULL; if(fresh.count && !targets) okay=0;
    for(size_t i=0;okay && i<fresh.count;++i) targets[i]=fresh.targets[i].target;
    int sources=0;for(size_t i=0;i<fresh.count;++i)if(xidePathHasExtension(fresh.targets[i].path,".xl")){sources=1;break;}
    uint64_t epoch=okay ? (s->holdToken ? xideFileWatchResume(s->watch,s->holdToken,targets,fresh.count,sources,e) : xideFileWatchSetTargetsWithSources(s->watch,targets,fresh.count,sources,e)) : 0; free(targets);
    if(!epoch) { clearTargets(&fresh); return fail(e,XIDE_MODEL_MEMORY,"Cannot update file watch subscriptions; current subscriptions retained."); }
    uint64_t previousDocumentEpoch=s->documentEpoch;s->documentEpoch=fresh.documentEpoch;
    clearTargets(s); s->targets=fresh.targets; s->count=fresh.count; s->capacity=fresh.capacity; s->nextToken=fresh.nextToken; s->epoch=epoch; s->fingerprint=f;
    for(size_t i=0;i<s->changeCount;) {
        xide_file_change* c=&s->changes[i]; subscription* match=NULL;
        for(size_t j=0;j<s->count;++j) if((s->targets[j].documentId==c->documentId || ((c->roles&s->targets[j].roles)&(WORKSPACE|PROJECT))) && !strcmp(s->targets[j].path,c->snapshot.path)) { match=&s->targets[j]; break; }
        if(!match || previousDocumentEpoch!=s->documentEpoch) removeChange(s,i); else { c->snapshot.token=match->target.token;c->snapshot.epoch=s->holdToken||!c->snapshot.epoch?0:epoch;c->documentId=match->documentId;c->roles=match->roles;++i; }
    }
    ++s->revision; return 1;
}
int xideFileSyncRebind(xide_app* app,xide_model_error* e)
{ if(xideDocumentsSourcesHeld(&app->documents) || (app->fileSync&&app->fileSync->holdToken))return fail(e,XIDE_MODEL_CONFLICT,"File synchronization is suspended for a source transaction.");return !app->fileSync||subscribe(app,e); }
uint64_t xideFileSyncSuspend(xide_app* app,xide_model_error* e)
{
    xide_file_sync* s=app?app->fileSync:NULL;
    if(!s){fail(e,XIDE_MODEL_INVALID,"Missing file synchronization service.");return 0;}
    if(s->holdToken){fail(e,XIDE_MODEL_CONFLICT,"File synchronization is already suspended.");return 0;}
    uint64_t token=xideFileWatchSuspend(s->watch,e);if(token){s->holdToken=token;++s->revision;}return token;
}
int xideFileSyncSuspended(xide_app* app,uint64_t token)
{xide_file_sync* s=app?app->fileSync:NULL;return s&&token&&s->holdToken==token&&xideFileWatchSuspended(s->watch,token);}
int xideFileSyncResume(xide_app* app,uint64_t token,xide_model_error* e)
{
    xide_file_sync* s=app?app->fileSync:NULL;
    if(!s||!token||s->holdToken!=token||!xideFileWatchSuspended(s->watch,token))return fail(e,XIDE_MODEL_CONFLICT,"File synchronization lease is stale or not acknowledged.");
    uint64_t oldEpoch=s->epoch;s->epoch=0;
    if(!subscribe(app,e)){s->epoch=oldEpoch;return 0;}
    s->holdToken=0;return 1;
}
void xideFileSyncPoll(xide_app* app)
{
    xide_file_sync* s=app->fileSync; if(!s||s->holdToken || xideDocumentsSourcesHeld(&app->documents)) return; xide_model_error error={0};
    if(!subscribe(app,&error)) { snprintf(s->status,sizeof(s->status),"%.180s",error.message); xuiStatusBarSetItemText(app->statusBar,0,s->status); return; }
    size_t bytes=0; int count=0; xide_watch_event event={0};
    while(count++<16 && bytes<4u*1024u*1024u && xideFileWatchTake(s->watch,&event)) {
        if(event.epoch==s->epoch && !event.token && (event.state==XIDE_WATCH_SOURCE_CHANGE || event.state==XIDE_WATCH_SOURCE_RESCAN)) {
            ++s->revision;
            if(event.state==XIDE_WATCH_SOURCE_RESCAN) {
                /* Every checked entry includes its own source in its guard.
                   Touch subscribed xlang roots to revalidate import candidates
                   after directory changes/coverage gaps,without reading bodies
                   or restarting the connection for an ordinary small cohort. */
                for(size_t i=0;i<s->count;++i)if(xidePathHasExtension(s->targets[i].path,".xl"))xideLanguageViewDiskChanged(app,s->targets[i].path,2);
            } else {xideTasksInvalidateInput(app->tasks,event.path);xideLanguageViewDiskChanged(app,event.path,event.changeType);}
            xideCompletionViewDiskChanged(app);
            xideFileWatchEventClear(&event);continue;
        }
        bytes+=event.length; subscription* t=event.epoch==s->epoch ? targetByToken(s,event.token) : NULL;
        if(t) {
            /* A snapshot begun before our successful save cannot describe the
               new baseline. Discard it rather than reloading or cancelling. */
            if(t->savedAt && event.observedAt<=t->savedAt) {
                xideFileWatchEventClear(&event);xideFileWatchRescan(s->watch);continue;
            }
            int previous=t->last.state;int changed=t->seen && !sameStamp(&t->last,&event); t->last=event; t->last.bytes=NULL; t->last.path=NULL; t->seen=1;
            /* Atomic saves change file identity/time before their delayed
               watch event reaches the UI. The saved document/model is already
               the new baseline; only unknown contents invalidate a new task. */
            int transient=event.state==XIDE_WATCH_UNSTABLE || (event.state==XIDE_WATCH_UNREADABLE &&
                (event.error==ERROR_SHARING_VIOLATION || event.error==ERROR_LOCK_VIOLATION || event.error==ERROR_DELETE_PENDING));
            if(changed && !transient && !alreadyKnown(app,t,&event)) {
                ++s->revision;xideTasksInvalidateInput(app->tasks,event.path);
                xideLanguageViewDiskChanged(app,event.path,event.state==XIDE_WATCH_MISSING ? 3 : previous==XIDE_WATCH_MISSING ? 1 : 2);
                xideCompletionViewDiskChanged(app);
            }
            if(t->roles&(DOCUMENT|WORKSPACE|PROJECT)) {
                int kept=0; for(size_t i=0;i<s->changeCount;++i) if(s->changes[i].acknowledged && s->changes[i].snapshot.token==event.token && sameStamp(&s->changes[i].snapshot,&event)) { kept=1; break; }
                if(kept) { xideFileWatchEventClear(&event); continue; }
                int okay=0; memset(&error,0,sizeof(error));
                if(!paused(app)) okay=apply(app,t,&event,0,&error); else okay=alreadyKnown(app,t,&event);
                if(okay) { for(size_t i=0;i<s->changeCount;++i) if(s->changes[i].snapshot.token==event.token) { removeChange(s,i); break; } }
                else { xide_document* d=documentById(app,t->documentId); if(d) d->externalState=event.state; if(!storeChange(s,t,&event,error.message[0] ? error.message : "A modal editor is open; disk coordination is deferred.")) s->retryDropped=1;
                    for(size_t i=0;i<s->changeCount;++i) if(s->changes[i].snapshot.token==t->target.token) s->changes[i].lastAttempt=paused(app) ? 0 : localSignature(app,t->documentId); }
            } else if(changed) s->refreshTree=1;
        }
        xideFileWatchEventClear(&event);
    }
    if(!paused(app)) {
        for(size_t i=0;i<s->changeCount;) {
            xide_file_change* c=&s->changes[i]; subscription* t=targetByToken(s,c->snapshot.token);
            if(!t) { removeChange(s,i); continue; }
            uint64_t local=localSignature(app,t->documentId);
            if(c->snapshot.epoch!=s->epoch || c->acknowledged || c->lastAttempt==local) { ++i; continue; }
            c->lastAttempt=local; memset(&error,0,sizeof(error));
            if(!c->acknowledged && apply(app,t,&c->snapshot,0,&error)) { removeChange(s,i); continue; }
            if(error.message[0]) snprintf(c->reason,sizeof(c->reason),"%s",error.message);
            ++i;
        }
        if(s->refreshTree && !xideWorkspaceViewHasDraft(app)) { xvalue* reading=xideReadingCapture(app,NULL); if(reading) { refreshTree(app,reading); xrtValueRelease(reading); s->refreshTree=0; } }
    }
    if(s->retryDropped && GetTickCount64()>=s->nextRescan) { xideFileWatchRescan(s->watch); s->nextRescan=GetTickCount64()+2000; }
    size_t pending=xideFileSyncCount(app); xide_watch_stats stats={0}; xideFileWatchStats(s->watch,&stats);
    if(stats.dropped!=s->languageDropped || s->dropped!=s->languageStoreDropped) {
        s->languageDropped=stats.dropped;s->languageStoreDropped=s->dropped;++s->revision;
        xideLanguageViewDiskChanged(app,NULL,0);xideCompletionViewDiskChanged(app);xideFileWatchRescan(s->watch);
    }
    if(pending || s->dropped || stats.dropped) { snprintf(s->status,sizeof(s->status),"磁盘变化：%zu 待处理%s",pending,s->dropped || stats.dropped ? " · 队列曾达上限，请刷新检查" : ""); xuiStatusBarSetItemText(app->statusBar,0,s->status); }
}
int xideFileSyncSettled(xide_app* app)
{ if(!app->fileSync) return 1;if(app->fileSync->holdToken || xideDocumentsSourcesHeld(&app->documents) || app->fileSync->documentEpoch!=xideDocumentsSourceEpoch(&app->documents))return 0;xide_watch_stats s={0};xideFileWatchStats(app->fileSync->watch,&s);return app->fileSync->epoch&&s.readyEpoch==app->fileSync->epoch&&!s.queued; }
size_t xideFileSyncCount(xide_app* app)
{ size_t n=0; if(app->fileSync) for(size_t i=0;i<app->fileSync->changeCount;++i) if(!app->fileSync->changes[i].acknowledged) ++n; return n; }
const xide_file_change* xideFileSyncAt(xide_app* app,size_t index)
{ if(app->fileSync) for(size_t i=0;i<app->fileSync->changeCount;++i) if(!app->fileSync->changes[i].acknowledged && !index--) return &app->fileSync->changes[i]; return NULL; }
uint64_t xideFileSyncRevision(xide_app* app) { return app->fileSync ? app->fileSync->revision : 0; }
void xideFileSyncRefresh(xide_app* app)
{ if(app->fileSync) { for(size_t i=0;i<app->fileSync->changeCount;++i) app->fileSync->changes[i].acknowledged=0; ++app->fileSync->revision; xideFileWatchRescan(app->fileSync->watch); } }
int xideFileSyncKeep(xide_app* app,uint64_t token,uint64_t sequence)
{ if(app->fileSync&&!app->fileSync->holdToken&&!xideDocumentsSourcesHeld(&app->documents)) for(size_t i=0;i<app->fileSync->changeCount;++i) { xide_file_change* c=&app->fileSync->changes[i]; if(c->snapshot.epoch==app->fileSync->epoch&&c->snapshot.token==token && c->snapshot.sequence==sequence) { c->acknowledged=1; ++app->fileSync->revision; return 1; } } return 0; }
int xideFileSyncReload(xide_app* app,uint64_t token,uint64_t sequence,uint32_t version,int discard,xide_model_error* e)
{
    xide_file_sync* s=app->fileSync;
    if(xideDocumentsSourcesHeld(&app->documents) || (s&&s->holdToken))return fail(e,XIDE_MODEL_CONFLICT,"File synchronization is suspended for a source transaction.");
    if(s) for(size_t i=0;i<s->changeCount;++i) {
        xide_file_change* c=&s->changes[i]; subscription* t=targetByToken(s,token);
        if(c->snapshot.epoch!=s->epoch || c->snapshot.token!=token || c->snapshot.sequence!=sequence || !t) continue;
        xide_document* d=documentById(app,t->documentId);
        if(d && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))!=version) return fail(e,XIDE_MODEL_CONFLICT,"Local document changed since review; select and compare the current versions again.");
        if(!apply(app,t,&c->snapshot,discard,e)) return 0;
        removeChange(s,i); return 1;
    }
    return fail(e,XIDE_MODEL_CONFLICT,"Disk selection changed or was closed; select and review the current versions again.");
}
int xideFileSyncBeforeSave(xide_document* document,const char* target,xide_model_error* e,void* user)
{
    xide_app* app=user;if(app->fileSync&&app->fileSync->holdToken)return fail(e,XIDE_MODEL_CONFLICT,"Saving is paused for a source transaction.");if(!isManifest(app,target)) return 1;
    const char* text=xuiCodeEditGetText(document->editor); xvalue* root=json((const unsigned char*)text,strlen(text),e); if(!root) return 0;
    int okay=coordinate(app,target,root,0,0,e); xrtValueRelease(root); return okay;
}
void xideFileSyncAfterSave(xide_document* document,void* user)
{
    xide_app* app=user;xide_file_sync* s=app->fileSync;
    if(s) for(size_t i=0;i<s->count;++i) {
        subscription* t=&s->targets[i];if(t->documentId!=document->id || strcmp(t->path,document->path))continue;
        t->savedAt=GetTickCount64();
        for(size_t j=0;j<s->changeCount;) {
            if(s->changes[j].snapshot.token==t->target.token && s->changes[j].snapshot.observedAt<=t->savedAt)removeChange(s,j);
            else ++j;
        }
    }
    if(!isManifest(app,document->path)) return;
    xide_model_error e={0}; xvalue* root=json(document->baseline.raw,document->baseline.rawLength,&e);
    int okay=root && coordinate(app,document->path,root,0,1,&e); xrtValueRelease(root);
    if(!okay) { fprintf(stderr,"saved manifest synchronization: %s\n",e.message); xideFileSyncRefresh(app); }
}
