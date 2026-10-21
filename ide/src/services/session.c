#include "services/session.h"
#include "services/rename_journal.h"
#include "services/migration_journal.h"
#include "services/recovery.h"
#include "ui/reading_state.h"
#include "ui/recovery_view.h"
#include "platform/state_file.h"
#include "platform/recovery_store.h"
#include "platform/window_state.h"
#include "platform/path.h"
#include "editor/coordinates.h"
#include "lang/languages.h"
#include <math.h>
#include "ui/layout_state.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/rename_view.h"
#include "ui/catalog_view.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
struct xide_session {
    xide_state_file metadata,layout,index;
    xide_window_state window;
    char *globalDirectory,*directory,*base;
    xvalue *unavailable,*legacyHints;
    xide_rename_journal* renameJournal;
    xide_migration_journal *migrationJournal,*incomingMigration;
    xide_recovery_service* migrationMaintenance;
    xvalue *incomingCheckpoint,*restoredCheckpoint;
    int ready,layoutReady,indexReady,restoring,blocked,scrollPending;
    uint64_t fingerprint,due,lastMaintenance,lastMigrationMaintenance;
    char issue[512];
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static const char* string(const xvalue* v,const char* key,const char* fallback) { const char* s=text(get(v,key)); return s ? s : fallback; }
static char* copy(const char* text) { size_t n=strlen(text)+1; char* p=malloc(n); if(p) memcpy(p,text,n); return p; }
static int integer(const xvalue* v,int fallback)
{
    int64 n=0; uint64 u=0;
    if(xrtValueGetInt(v,&n) && n>=0 && n<=INT_MAX) return (int)n;
    if(xrtValueGetUInt(v,&u) && u<=INT_MAX) return (int)u;
    return fallback;
}
static int numeric(const xvalue* value,double* result)
{
    int64 i=0; uint64 u=0;
    if(xrtValueGetFloat(value,result)) return 1;
    if(xrtValueGetInt(value,&i)) { *result=(double)i; return 1; }
    if(xrtValueGetUInt(value,&u)) { *result=(double)u; return 1; }
    return 0;
}
static int set(xvalue* v,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(v,sv(key),value); }
static int take(xvalue* object,const char* key,xvalue** value) { xvalue* owned=*value; *value=NULL; return set(object,key,owned); }
static int append(xvalue* array,xvalue** value) { xvalue* owned=*value; *value=NULL; return owned && xrtValueArrayAppendNew(array,owned); }
static int setText(xvalue* v,const char* key,const char* value) { return set(v,key,xrtValueString(sv(value ? value : ""))); }
static int fail(xide_model_error* e,int code,const char* message) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static void report(xide_app* app,const char* message)
{
    xide_session* s=app->session; if(!s || !message) return;
    if(strcmp(s->issue,message)) { snprintf(s->issue,sizeof(s->issue),"%s",message); fprintf(stderr,"session: %s\n",message); }
    if(app->statusBar) xuiStatusBarSetItemText(app->statusBar,1,s->issue);
}
static uint64_t hash(uint64_t value,const void* bytes,size_t count)
{ const unsigned char* p=bytes; for(size_t i=0;i<count;++i) { value^=p[i]; value*=UINT64_C(1099511628211); } return value; }
static uint64_t hashText(uint64_t h,const char* text) { return hash(h,text ? text : "",text ? strlen(text) : 0); }
static uint64_t current(xide_app* app)
{
    uint64_t h=UINT64_C(1469598103934665603);
    h=hashText(h,app->workspaceModel.manifest); h=hashText(h,app->projectSessionPath); h=hashText(h,app->selectedProject);
    h=hash(h,&app->theme,sizeof(app->theme)); int layout=xuiDockPanelGetLayoutChangeCount(app->dock); h=hash(h,&layout,sizeof(layout));
    h=hashText(h,xuiEditGetText(xideCatalogSearchWidget(app)));
    if(app->selectedCommand>=0 && (size_t)app->selectedCommand<app->catalog.count) h=hashText(h,app->catalog.entries[app->selectedCommand].id);
    if(app->activeDocument) h=hashText(h,app->activeDocument->sessionId);
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* d=app->documents.items[i]; if(!d->open) continue;
        h=hashText(h,d->sessionId); h=hashText(h,d->path); h=hashText(h,d->language);
        h=hashText(h,text(get(d->cAnalysisContext,"project"))); h=hashText(h,text(get(d->cAnalysisContext,"source"))); h=hashText(h,text(get(d->cAnalysisContext,"sourceId")));
        uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); h=hash(h,&version,sizeof(version));
        xui_code_selection_t selection={0}; xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection);
        h=hash(h,&selection.iCaretOffset,sizeof(selection.iCaretOffset)); h=hash(h,&selection.iAnchorOffset,sizeof(selection.iAnchorOffset));
        float scrollX=0,scrollY=0; xuiCodeEditGetScroll(d->editor,&scrollX,&scrollY);
        h=hash(h,&scrollX,sizeof(scrollX)); h=hash(h,&scrollY,sizeof(scrollY));
        xui_code_fold_state folds=xuiCodeEditGetFoldState(d->editor);
        for(int k=0;k<xuiCodeFoldStateGetCount(folds);++k) { xui_code_fold_range_t range={0}; range.iSize=sizeof(range); xuiCodeFoldStateGetRange(folds,k,&range); h=hash(h,&range.iStartLine,sizeof(range.iStartLine)); h=hash(h,&range.iFlags,sizeof(range.iFlags)); }
    }
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) h=hashText(h,app->workspaceModel.projects[i].activeConfiguration);
    return xideReadingFingerprint(app,h);
}
static const char* owner(xide_app* app) { return app->workspaceModel.manifest ? app->workspaceModel.manifest : app->projectSessionPath; }
static const char* ownerKind(xide_app* app) { return app->workspaceModel.manifest ? "workspace" : app->projectSessionPath ? "project" : "none"; }
static int validIndex(const xvalue* root)
{
    const char *kind=text(get(root,"kind")),*path=text(get(root,"path"));
    return kind && path && ((!strcmp(kind,"none") && !*path) || ((!strcmp(kind,"workspace") || !strcmp(kind,"project")) && *path));
}
static xvalue* captureIndex(xide_app* app)
{
    xide_session* s=app->session; xvalue* root=s && s->indexReady ? xrtValueDeepClone(s->index.source) : xrtValueObject();
    if(!root || !setText(root,"format","xlang-ide-session-index-1") || !setText(root,"kind",ownerKind(app)) || !setText(root,"path",owner(app))) { xrtValueRelease(root); return NULL; }
    return root;
}
static char* globalDirectory(xide_app* app)
{
    if(app->sessionRoot) return xidePathResolve(NULL,app->sessionRoot);
    DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",NULL,0); if(!length) return NULL;
    wchar_t* wide=malloc(length*sizeof(*wide)); if(!wide) return NULL; GetEnvironmentVariableW(L"LOCALAPPDATA",wide,length);
    int size=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL); char* utf8=size ? malloc(size) : NULL;
    if(utf8) WideCharToMultiByte(CP_UTF8,0,wide,-1,utf8,size,NULL,NULL);
    free(wide);
    char* path=utf8 ? xidePathResolve(utf8,"xlang-ide") : NULL; free(utf8); return path;
}
static int safeId(const char* s)
{ if(!s || !*s || strlen(s)>100) return 0; for(;*s;++s) if(!isalnum((unsigned char)*s) && *s!='-' && *s!='_') return 0; return 1; }
static int recoveryRef(const char* s)
{ if(!s || strncmp(s,"recovery/",9) || !s[9] || !strcmp(s+9,".") || !strcmp(s+9,"..")) return 0; for(s+=9;*s;++s) if(!isalnum((unsigned char)*s) && *s!='-' && *s!='_' && *s!='.') return 0; return 1; }
static int validSession(const xvalue* root)
{
    bool pending=false; if(get(root,"renameResourcesPending") && !xrtValueGetBool(get(root,"renameResourcesPending"),&pending)) return 0;
    xvalue* docs=get(root,"openDocuments");
    if(docs && (!xrtValueIs(docs,XVALUE_ARRAY) || xrtValueCount(docs)>10000)) return 0;
    for(size_t i=0;i<xrtValueCount(docs);++i) {
        xvalue* d=xrtValueArrayGet(docs,i); const char* path=text(get(d,"path")); const char* language=string(d,"language","text"); const char* id=text(get(d,"id"));
        if(!xrtValueIs(d,XVALUE_OBJECT) || !path || (id && !safeId(id)) || (strcmp(language,"c") && strcmp(language,"xlang") && strcmp(language,"text")) || integer(get(d,"line"),1)<1 || integer(get(d,"column"),1)<1) return 0;
        const char* positions[]={"line","column","anchorLine","anchorColumn"};
        for(int k=0;k<4;++k) if(get(d,positions[k]) && integer(get(d,positions[k]),-1)<1) return 0;
        if(get(d,"id") && !safeId(text(get(d,"id")))) return 0;
        if(get(d,"title") && !text(get(d,"title"))) return 0;
        if(get(d,"language") && !text(get(d,"language"))) return 0;
        const char* scroll[]={"scrollX","scrollY"};
        for(int k=0;k<2;++k) if(get(d,scroll[k])) { double n=0; if(!numeric(get(d,scroll[k]),&n) || !isfinite(n) || n<0 || n>10000000) return 0; }
        if(get(d,"collapsedFolds") && (!xrtValueIs(get(d,"collapsedFolds"),XVALUE_ARRAY) || xrtValueCount(get(d,"collapsedFolds"))>10000)) return 0;
        for(size_t k=0;k<xrtValueCount(get(d,"collapsedFolds"));++k) {
            const xvalue* fold=xrtValueArrayGet(get(d,"collapsedFolds"),k);
            if(!xrtValueIs(fold,XVALUE_OBJECT) || integer(get(fold,"line"),0)<1 || integer(get(fold,"endLine"),0)<=integer(get(fold,"line"),0)) return 0;
        }
        if(!xideDocumentCContextValid(get(d,"cAnalysisContext"))) return 0;
        bool override=false; if(get(d,"languageOverride") && !xrtValueGetBool(get(d,"languageOverride"),&override)) return 0;
        if(get(d,"recoveryBaseline") && !get(d,"recoveryText")) return 0;
        if(get(d,"recoveryText") && (!recoveryRef(text(get(d,"recoveryText"))) || !recoveryRef(text(get(d,"recoveryBaseline"))))) return 0;
        for(size_t j=0;id && j<i;++j) if(!strcmp(id,string(xrtValueArrayGet(docs,j),"id",""))) return 0;
    }
    if(get(root,"configurations") && (!xrtValueIs(get(root,"configurations"),XVALUE_ARRAY) || xrtValueCount(get(root,"configurations"))>10000)) return 0;
    for(size_t i=0;i<xrtValueCount(get(root,"configurations"));++i) {
        const xvalue* entry=xrtValueArrayGet(get(root,"configurations"),i);
        if(!xrtValueIs(entry,XVALUE_OBJECT) || !text(get(entry,"id")) || !text(get(entry,"manifest")) || !text(get(entry,"configuration"))) return 0;
    }
    const char* fields[]={"activeProject","activeDocument","activeDocumentId","selectedCommand","commandSearch","theme","ownerKind","ownerPath","coordinateSystem","layoutGeneration"};
    for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) if(get(root,fields[i]) && !text(get(root,fields[i]))) return 0;
    return xideReadingValid(get(root,"reading"));
}
int xideSessionValidate(const xvalue* root) { return root && validSession(root); }
static int validLayout(const xvalue* root)
{
    const xvalue* states=get(root,"states");
    if(!xrtValueIs(states,XVALUE_ARRAY) || xrtValueCount(states)>64) return 0;
    for(size_t i=0;i<xrtValueCount(states);++i) {
        const xvalue* state=xrtValueArrayGet(states,i); const char* id=text(get(state,"id"));
        if(!xrtValueIs(state,XVALUE_OBJECT) || !safeId(id) || !xrtValueIs(get(state,"dock"),XVALUE_OBJECT)) return 0;
        for(size_t j=0;j<i;++j) if(!strcmp(id,string(xrtValueArrayGet(states,j),"id",""))) return 0;
    }
    return 1;
}
static void clearScope(xide_session* s)
{
    xideRenameJournalDestroy(s->renameJournal); s->renameJournal=NULL;
    xideRenameJournalDestroy(s->migrationJournal); s->migrationJournal=NULL;
    xrtValueRelease(s->restoredCheckpoint); s->restoredCheckpoint=NULL;
    xideStateClear(&s->metadata); xideStateClear(&s->layout); free(s->directory); free(s->base);
    xrtValueRelease(s->legacyHints); s->legacyHints=NULL;
    s->directory=NULL; s->base=NULL; s->ready=0; s->layoutReady=0; s->blocked=0; s->lastMaintenance=0; xrtValueRelease(s->unavailable); s->unavailable=xrtValueArray();
}
static int configure(xide_app* app)
{
    xide_session* s=app->session; clearScope(s); const char* path=owner(app);
    s->base=path ? xidePathDirectory(path) : copy(s->globalDirectory);
    uint64_t code=hashText(UINT64_C(1469598103934665603),ownerKind(app));
    char* lower=path ? copy(app->sessionRoot ? path : xidePathFilename(path)) : copy("global");
    if(!s->base || !lower) { free(lower); return 0; }
    for(char* p=lower;*p;++p) if((unsigned char)*p<128) *p=(char)tolower((unsigned char)*p);
    code=hashText(code,lower); free(lower);
    char name[120]; snprintf(name,sizeof(name),app->sessionRoot ? "workspaces/%016llx" : ".xlang-ide/workspaces/%016llx",(unsigned long long)code);
    s->directory=path ? xidePathResolve(app->sessionRoot ? s->globalDirectory : s->base,name) : xidePathResolve(s->globalDirectory,"global");
    if(!s->directory) return 0;
    char* metadata=xidePathResolve(s->directory,"session.json"),*layout=xidePathResolve(s->directory,"layout.json"); xide_model_error e={0};
    s->ready=metadata && xideStateOpen(&s->metadata,metadata,"xlang-ide-session-1",&e);
    if(s->ready && !validSession(s->metadata.source)) { s->ready=0; fail(&e,XIDE_MODEL_INVALID,"Session document metadata is invalid; the original is preserved."); }
    if(!s->ready) report(app,e.message);
    s->layoutReady=layout && xideStateOpen(&s->layout,layout,"xlang-ide-layout-1",&e);
    if(s->layoutReady && s->layout.existed && !validLayout(s->layout.source)) { s->layoutReady=0; fail(&e,XIDE_MODEL_INVALID,"Layout metadata is invalid; the original is preserved."); }
    if(!s->layoutReady) report(app,e.message);
    free(metadata); free(layout); return 1;
}
static const xvalue* oldEntry(xide_session* s,const char* id)
{
    xvalue* docs=get(s->restoredCheckpoint ? s->restoredCheckpoint : s->metadata.source,"openDocuments");
    for(size_t i=0;i<xrtValueCount(docs);++i) { const xvalue* d=xrtValueArrayGet(docs,i); if(!strcmp(id,string(d,"id",""))) return d; }
    return NULL;
}
static char* snapshot(xide_session* s,const char* id,const char* kind,const unsigned char* bytes,size_t length,xide_model_error* error)
{
    uint64_t code=hash(UINT64_C(1469598103934665603),bytes,length); char relative[190];
    snprintf(relative,sizeof(relative),"recovery/%s-%016llx.%s",id,(unsigned long long)code,kind);
    char* path=xideRecoveryPath(s->directory,relative,error); if(!path) return NULL;
    if(xidePathExists(path)) {
        char* native=xidePathNativeUtf8(path); size_t size=0;
        unsigned char* old=native ? xrtFileReadAllLimit(native,16u*1024u*1024u+4u,&size) : NULL; free(native);
        int same=old && size==length && (!length || !memcmp(old,bytes,length)); xrtFree(old);
        if(same) { free(path); return copy(relative); }
        free(path); char* unique=xideCreateId("snap"); if(!unique) return NULL;
        snprintf(relative,sizeof(relative),"recovery/%s.%s",unique,kind); free(unique); path=xideRecoveryPath(s->directory,relative,error);
    }
    char* directory=path ? xidePathDirectory(path) : NULL;
    char* nativeDir=directory ? xidePathNativeUtf8(directory) : NULL; char* nativePath=path ? xidePathNativeUtf8(path) : NULL;
    int okay=nativeDir && nativePath && xrtDirCreateAll(nativeDir) && xrtFileWriteAtomic(nativePath,(xbytesview){bytes,length});
    free(nativeDir); free(nativePath); free(directory); free(path);
    if(!okay) { fail(error,XIDE_MODEL_IO,"Cannot save recovery text; the previous session is preserved."); return NULL; }
    return copy(relative);
}
static xvalue* captureDocument(xide_app* app,xide_document* d,int cleanExit,size_t* recoveryBytes,xide_model_error* error)
{
    xide_session* s=app->session; const xvalue* old=oldEntry(s,d->sessionId);
    for(size_t i=0;!old && d->path && i<xrtValueCount(get(s->metadata.source,"openDocuments"));++i) {
        const xvalue* candidate=xrtValueArrayGet(get(s->metadata.source,"openDocuments"),i);
        const char* relative=string(candidate,"path",""); char* path=*relative ? xidePathResolve(s->base,relative) : NULL;
        if(path && xidePathEqual(path,d->path)) old=candidate;
        free(path);
    }
    xvalue* entry=old ? xrtValueDeepClone(old) : xrtValueObject();
    char* path=d->path ? xidePathRelative(s->base,d->path) : copy(""); if(!entry || !path) { xrtValueRelease(entry); free(path); return NULL; }
    const char* content=xuiCodeEditGetText(d->editor); xui_code_selection_t selection={0}; xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection);
    int line=1,column=1,anchorLine=1,anchorColumn=1; xidePositionFromOffset(content,selection.iCaretOffset,&line,&column); xidePositionFromOffset(content,selection.iAnchorOffset,&anchorLine,&anchorColumn);
    int okay=setText(entry,"id",d->sessionId) && setText(entry,"path",path) && setText(entry,"title",d->title) && setText(entry,"language",d->language) &&
        set(entry,"languageOverride",xrtValueBool(d->languageOverride!=0)) && set(entry,"line",xrtValueInt(line)) && set(entry,"column",xrtValueInt(column)) &&
        set(entry,"anchorLine",xrtValueInt(anchorLine)) && set(entry,"anchorColumn",xrtValueInt(anchorColumn));
    float scrollX=0,scrollY=0; xuiCodeEditGetScroll(d->editor,&scrollX,&scrollY);
    xvalue* collapsed=xrtValueArray(); xui_code_fold_state folds=xuiCodeEditGetFoldState(d->editor);
    for(int i=0;okay && i<xuiCodeFoldStateGetCount(folds);++i) {
        xui_code_fold_range_t range={0}; range.iSize=sizeof(range); xuiCodeFoldStateGetRange(folds,i,&range);
        if(range.iFlags & XUI_CODE_FOLD_COLLAPSED) {
            xvalue* fold=xrtValueObject();
            okay=fold && set(fold,"line",xrtValueInt(range.iStartLine+1)) && set(fold,"endLine",xrtValueInt(range.iEndLine+1)) && append(collapsed,&fold);
            xrtValueRelease(fold);
        }
    }
    okay=okay && collapsed && set(entry,"scrollX",xrtValueFloat(scrollX)) && set(entry,"scrollY",xrtValueFloat(scrollY)) && take(entry,"collapsedFolds",&collapsed);
    xrtValueRelease(collapsed);
    xrtValueObjectRemove(entry,sv("cAnalysisContext"));
    if(d->cAnalysisContext) okay=okay && xideDocumentCContextValid(d->cAnalysisContext) && set(entry,"cAnalysisContext",xrtValueDeepClone(d->cAnalysisContext));
    free(path); xrtValueObjectRemove(entry,sv("recoveryText")); xrtValueObjectRemove(entry,sv("recoveryBaseline"));
    if(okay && (!d->path || (d->dirty && !cleanExit))) {
        size_t length=strlen(content); *recoveryBytes+=length+d->baseline.rawLength;
        if(length>16u*1024u*1024u || *recoveryBytes>128u*1024u*1024u) { fail(error,XIDE_MODEL_INVALID,"Recovery snapshot exceeds the current 128 MiB budget."); okay=0; }
        char* body=okay ? snapshot(s,d->sessionId,"text",(const unsigned char*)content,length,error) : NULL;
        char* baseline=body ? snapshot(s,d->sessionId,"base",d->baseline.raw,d->baseline.rawLength,error) : NULL;
        okay=body && baseline && setText(entry,"recoveryText",body) && setText(entry,"recoveryBaseline",baseline); free(body); free(baseline);
    }
    if(!okay) { xrtValueRelease(entry); return NULL; } return entry;
}
static xvalue* configurations(xide_app* app)
{
    xvalue* array=xrtValueArray(); if(!array) return NULL;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
        xide_project* p=&app->workspaceModel.projects[i]; char* path=xidePathRelative(app->session->base,p->manifest); const xvalue* old=NULL;
        const xvalue* before=get(app->session->metadata.source,"configurations");
        for(size_t j=0;!old && j<xrtValueCount(before);++j) {
            const xvalue* candidate=xrtValueArrayGet(before,j);
            if(!strcmp(p->id,string(candidate,"id","")) || (path && !strcmp(path,string(candidate,"manifest","")))) old=candidate;
        }
        xvalue* entry=old ? xrtValueDeepClone(old) : xrtValueObject();
        int okay=entry && path && setText(entry,"id",p->id) && setText(entry,"manifest",path) && setText(entry,"configuration",p->activeConfiguration);
        free(path); if(!okay || !append(array,&entry)) { xrtValueRelease(entry); xrtValueRelease(array); return NULL; }
    }
    return array;
}
static const xvalue* layoutState(xide_session* s,const char* generation)
{
    const xvalue* states=get(s->layout.source,"states");
    for(size_t i=0;i<xrtValueCount(states);++i) { const xvalue* state=xrtValueArrayGet(states,i); if(!strcmp(generation,string(state,"id",""))) return state; }
    return NULL;
}

/* Merge extension fields only for retained objects. Window/region arrays use
   logical identity; pane tab arrays contain values and are replaced as a whole. */
static xvalue* preserveFields(const xvalue* fresh,const xvalue* old,int depth)
{
    if(depth>64) return xrtValueDeepClone(fresh);
    if(xrtValueIs(fresh,XVALUE_OBJECT) && xrtValueIs(old,XVALUE_OBJECT)) {
        const char* type=text(get(fresh,"type")),*before=text(get(old,"type"));
        if(type && before && strcmp(type,before)) return xrtValueDeepClone(fresh);
        xvalue* result=xrtValueDeepClone(old); if(!result) return NULL;
        for(size_t i=0;i<xrtValueCount(fresh);++i) {
            xstrview key={0}; const xvalue* value=xrtValueObjectAt(fresh,i,&key);
            xvalue* merged=preserveFields(value,xrtValueObjectGet(old,key),depth+1);
            if(!merged || !xrtValueObjectSetNew(result,key,merged)) { xrtValueRelease(result); return NULL; }
        }
        return result;
    }
    if(xrtValueIs(fresh,XVALUE_ARRAY) && xrtValueIs(old,XVALUE_ARRAY)) {
        xvalue* result=xrtValueArray(); if(!result) return NULL;
        for(size_t i=0;i<xrtValueCount(fresh);++i) {
            const xvalue* value=xrtValueArrayGet(fresh,i),*match=NULL;
            const char* identity=text(get(value,"id")); const char* field="id";
            if(!identity) { identity=text(get(value,"kind")); field="kind"; }
            for(size_t j=0;identity && j<xrtValueCount(old);++j) {
                const xvalue* candidate=xrtValueArrayGet(old,j);
                if(!strcmp(identity,string(candidate,field,""))) { match=candidate; break; }
            }
            xvalue* merged=preserveFields(value,match,depth+1);
            if(!merged || !xrtValueArrayAppendNew(result,merged)) { xrtValueRelease(result); return NULL; }
        }
        return result;
    }
    return xrtValueDeepClone(fresh);
}
static int checkpointLocked(xide_app* app,int cleanExit,xide_model_error* error,int* published)
{
    xide_session* s=app->session;
    xvalue *root=xrtValueDeepClone(s->metadata.source),*docs=xrtValueArray(); if(!root || !docs) { xrtValueRelease(root); xrtValueRelease(docs); return 0; }
    size_t bytes=0; int okay=1;
    for(size_t i=0;i<app->documents.count && okay;++i) {
        xide_document* d=app->documents.items[i]; if(!d->open || (cleanExit && d->dirty && !d->path)) continue;
        xvalue* entry=captureDocument(app,d,cleanExit,&bytes,error); okay=entry && xrtValueArrayAppendNew(docs,entry);
    }
    for(size_t i=0;okay && i<xrtValueCount(s->unavailable);++i) {
        const xvalue* missing=xrtValueArrayGet(s->unavailable,i); char* path=xidePathResolve(s->base,string(missing,"path",""));
        int opened=path && xideDocumentsFind(&app->documents,path); free(path);
        for(size_t j=0;!opened && j<app->documents.count;++j) if(app->documents.items[j]->open && !strcmp(app->documents.items[j]->sessionId,string(missing,"id",""))) opened=1;
        if(!opened) okay=xrtValueArrayAppendNew(docs,xrtValueDeepClone(missing));
    }
    const char* command=app->selectedCommand>=0 && (size_t)app->selectedCommand<app->catalog.count ? app->catalog.entries[app->selectedCommand].id : "";
    char* activePath=app->activeDocument && app->activeDocument->path ? xidePathRelative(s->base,app->activeDocument->path) : copy("");
    char* ownerPath=owner(app) ? xidePathRelative(s->base,owner(app)) : copy("");
    xvalue* configs=configurations(app);
    if(okay) okay=activePath && ownerPath && configs && setText(root,"format","xlang-ide-session-1") && setText(root,"coordinateSystem","unicode-codepoint-1") &&
        setText(root,"ownerKind",ownerKind(app)) && setText(root,"ownerPath",ownerPath) && setText(root,"activeDocument",activePath) &&
        setText(root,"activeDocumentId",app->activeDocument && !(cleanExit && app->activeDocument->dirty && !app->activeDocument->path) ? app->activeDocument->sessionId : "") &&
        setText(root,"activeProject",app->selectedProject) && setText(root,"selectedCommand",command) && setText(root,"commandSearch",xuiEditGetText(xideCatalogSearchWidget(app))) &&
        setText(root,"theme",ideThemeName(app->theme)) && set(root,"renameResourcesPending",xrtValueBool(xideRenameViewHasPending(app)!=0)) && take(root,"openDocuments",&docs) && take(root,"configurations",&configs);
    if(okay && s->legacyHints) okay=set(root,"legacyImportHints",xrtValueDeepClone(s->legacyHints));
    if(okay) okay=set(root,"reading",xideReadingCapture(app,get(s->metadata.source,"reading")));
    xrtValueRelease(docs); xrtValueRelease(configs);
    free(activePath); free(ownerPath);
    if(okay && s->layoutReady) {
        xvalue *layout=xrtValueDeepClone(s->layout.source),*states=xrtValueArray(),*state=xrtValueObject(),*dock=xideLayoutCapture(app,error); char* generation=xideCreateId("layout");
        const xvalue* previous=layoutState(s,string(s->metadata.source,"layoutGeneration",""));
        if(dock && previous) { xvalue* merged=preserveFields(dock,get(previous,"dock"),0); xrtValueRelease(dock); dock=merged; }
        okay=layout && states && state && dock && generation && setText(layout,"format","xlang-ide-layout-1") && setText(state,"id",generation) && take(state,"dock",&dock) && append(states,&state);
        if(okay && previous) okay=xrtValueArrayAppendNew(states,xrtValueDeepClone(previous));
        if(okay) okay=take(layout,"states",&states) && xideStateWrite(&s->layout,layout,error) && setText(root,"layoutGeneration",generation);
        xrtValueRelease(layout); xrtValueRelease(states); xrtValueRelease(state); xrtValueRelease(dock); free(generation);
    }
    if(okay) { okay=xideStateWrite(&s->metadata,root,error); if(okay) *published=1; }
    xrtValueRelease(root); if(!okay) { if(error && !error->message[0]) fail(error,XIDE_MODEL_MEMORY,"Cannot construct session snapshot."); return 0; }
    s->fingerprint=current(app); s->due=0; s->blocked=0;
    if(s->indexReady) {
        xvalue* index=captureIndex(app);
        okay=index && xideStateWrite(&s->index,index,error);
        xrtValueRelease(index);
    }
    return okay;
}
static int matches(xide_state_file* loaded,const char* directory,const char* leaf,const char* format,xide_model_error* e)
{
    char* path=xideRecoveryPath(directory,leaf,e); xide_state_file disk={0};
    if(!path) return 0;
    int okay=xideStateOpen(&disk,path,format,e); free(path);
    if(okay) okay=disk.existed==loaded->existed && (!disk.existed || xrtValueEqual(disk.source,loaded->source));
    xideStateClear(&disk);
    if(!okay && (!e || !e->message[0])) fail(e,XIDE_MODEL_CONFLICT,"Checkpoint changed on disk; reopen it before saving.");
    return okay;
}
static uint64_t fileTimeNow(void);
static int retainPrevious(xide_session* s,xide_recovery_guard* guard,xide_model_error* e)
{
    uint64_t now=fileTimeNow();
    if(s->metadata.existed && !xideRecoveryRetain(guard,s->metadata.source,now,e)) return -1;
    char* path=xideRecoveryPath(s->directory,"previous-session.json",e); if(!path) return 0;
    xide_state_file previous={0}; int okay=xideStateOpen(&previous,path,"xlang-ide-session-1",e); free(path);
    if(okay && previous.existed && !validSession(previous.source)) okay=fail(e,XIDE_MODEL_INVALID,"Previous checkpoint is damaged; it and all recovery files are preserved.");
    if(okay && previous.existed && !xideRecoveryRetain(guard,previous.source,now,e)) { xideStateClear(&previous); return -1; }
    if(okay && s->metadata.existed) okay=xideStateWrite(&previous,s->metadata.source,e);
    xideStateClear(&previous); return okay;
}
static uint64_t fileTimeNow(void)
{ FILETIME time; GetSystemTimeAsFileTime(&time); return ((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime; }
int xideSessionMaintenanceRoot(xide_app* app,char** root,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    *root=NULL;
    xide_session* s=app ? app->session : NULL;
    if(!s || !s->ready || s->restoring) return fail(error,XIDE_MODEL_INVALID,"Repair and reopen the session before maintenance.");
    if(xideRenameJournalPending(s->migrationJournal)) return fail(error,XIDE_MODEL_CONFLICT,"项目迁移仍待保存，恢复正文保留。");
    *root=copy(s->globalDirectory); return *root!=NULL || fail(error,XIDE_MODEL_MEMORY,"无法保留迁移维护目录。");
}
int xideSessionMaintain(xide_app* app,xide_recovery_stats* stats,xide_model_error* error)
{
    if(stats) memset(stats,0,sizeof(*stats));
    char* global=NULL;
    if(!xideSessionMaintenanceRoot(app,&global,error)) return 0;
    xide_session* s=app->session;
    xide_recovery_guard guard={0};
    if(!xideRecoveryLock(&guard,s->directory,error)) { free(global); return 0; }
    int okay=matches(&s->metadata,s->directory,"session.json","xlang-ide-session-1",error) &&
        xideRecoveryCollect(&guard,fileTimeNow(),validSession,stats,error);
    xideRecoveryUnlock(&guard);
    xide_recovery_stats migration={0}; if(okay) okay=xideMigrationJournalMaintain(global,fileTimeNow(),NULL,&migration,error);
    if(stats) { stats->scanned+=migration.scanned; stats->referenced+=migration.referenced; stats->retained+=migration.retained; stats->removed+=migration.removed; stats->skipped+=migration.skipped; stats->removedBytes+=migration.removedBytes; }
    free(global); if(!okay) report(app,error ? error->message : "Recovery cleanup was skipped."); return okay;
}
int xideSessionSave(xide_app* app,int cleanExit,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    xide_session* s=app ? app->session : NULL; if(!s || s->restoring) return 1;
    xide_model_error windowError={0}; if(!xideWindowStateSave(&s->window,&windowError)) report(app,windowError.message);
    if(!s->ready) return fail(error,XIDE_MODEL_INVALID,"Session is protected until it is repaired and reopened.");
    xide_recovery_guard guard={0}; if(!xideRecoveryLock(&guard,s->directory,error)) return 0;
    int okay=matches(&s->metadata,s->directory,"session.json","xlang-ide-session-1",error);
    if(okay && s->layoutReady) okay=matches(&s->layout,s->directory,"layout.json","xlang-ide-layout-1",error);
    int previousOkay=0,published=0; xide_model_error maintenanceError={0};
    if(okay) {
        previousOkay=retainPrevious(s,&guard,&maintenanceError);
        if(previousOkay<=0) report(app,maintenanceError.message);
        if(previousOkay<0) { okay=0; if(error) *error=maintenanceError; }
        else okay=checkpointLocked(app,cleanExit,error,&published);
    }
    if(published && !xideRenameJournalCheckpoint(s->renameJournal,app,s->metadata.source,&guard,error)) okay=0;
    if(published && !xideMigrationJournalCheckpoint(s->migrationJournal,app,s->directory,s->base,s->metadata.source,s->layout.source,error)) okay=0;
    if(published) { xrtValueRelease(s->restoredCheckpoint); s->restoredCheckpoint=NULL; }
    uint64_t now=GetTickCount64();
    if(published && previousOkay>0 && !xideRenameJournalPending(s->renameJournal) && !xideRenameJournalPending(s->migrationJournal) && (!s->lastMaintenance || now-s->lastMaintenance>=UINT64_C(3600000))) {
        s->lastMaintenance=now ? now : 1;
        xide_recovery_stats stats={0};
        if(!xideRecoveryCollect(&guard,fileTimeNow(),validSession,&stats,&maintenanceError)) report(app,maintenanceError.message);
    }
    xideRecoveryUnlock(&guard); return okay;
}
int xideSessionPrepareRename(xide_app* app,const char* source,const char* target,const xide_resource_rebind* resources,xide_model_error* error)
{
    xide_session* s=app ? app->session : NULL; if(!s) return 1;
    if(!s->ready || s->restoring) return fail(error,XIDE_MODEL_CONFLICT,"会话处于保护状态，请先处理会话存储后再改名。");
    if(!xideRenameJournalCancelPrepared(s->renameJournal,error)) return 0;
    if(xideRenameJournalPending(s->renameJournal)) {
        if(!xideSessionSave(app,0,error)) return 0;
    }
    xide_rename_journal* next=xideRenameJournalPrepare(app,s->directory,s->base,s->metadata.source,source,target,resources,error);
    if(!next) { return 0; } xideRenameJournalDestroy(s->renameJournal); s->renameJournal=next; return 1;
}
int xideSessionFinishRename(xide_app* app,const xide_resource_rebind* resources,xide_model_error* error)
{ return !app || !app->session || xideRenameJournalFinish(app->session->renameJournal,app,resources,error); }
void xideSessionCancelPreparedRename(xide_app* app)
{ xide_model_error e={0}; if(app && app->session && !xideRenameJournalCancelPrepared(app->session->renameJournal,&e)) report(app,e.message); }
int xideSessionBeforeProjectMigration(xide_app* app,xide_model_error* error)
{
    xide_session* s=app ? app->session : NULL; if(!s || !xideRenameJournalPending(s->renameJournal)) return 1;
    if(!xideSessionSave(app,0,error)) return 0;
    return !xideRenameJournalPending(s->renameJournal) || fail(error,XIDE_MODEL_CONFLICT,"请先保存文件改名涉及的工作空间、项目和资源文档，再迁移项目。");
}
int xideSessionPrepareProjectMigration(xide_app* app,const xide_project_migration* plan,xide_model_error* error)
{
 xide_session* s=app ? app->session : NULL; if(!s || !xideProjectMigrationPathCount(plan)) return 1;
 if(!s->ready || !s->layoutReady || s->restoring) return fail(error,XIDE_MODEL_CONFLICT,"会话存储处于保护状态，请先修复后再迁移项目。");
 xide_migration_journal* next=xideMigrationJournalPrepare(app,s->globalDirectory,s->directory,s->base,s->metadata.source,s->layout.source,plan,error);
 if(!next) return 0;
 xideRenameJournalDestroy(s->migrationJournal); s->migrationJournal=next; return 1;
}
void xideSessionCancelProjectMigration(xide_app* app)
{ xide_model_error e={0}; if(app && app->session && !xideMigrationJournalCancel(app->session->migrationJournal,&e)) report(app,e.message); }
void xideSessionDiscardMigration(xide_app* app)
{
 xide_session* s=app ? app->session : NULL; if(!s) return;
 xideRenameJournalDestroy(s->incomingMigration); s->incomingMigration=NULL; xrtValueRelease(s->incomingCheckpoint); s->incomingCheckpoint=NULL;
}
int xideSessionLoadMigration(xide_app* app,const char* path,int projectOnly,xide_workspace* model,char** projectPath,int* found,xide_model_error* error)
{
 *found=0; *projectPath=NULL; xide_session* s=app ? app->session : NULL; if(!s) return 1;
 xideSessionDiscardMigration(app);
 if(!xideMigrationJournalLoad(s->globalDirectory,path,projectOnly,model,projectPath,&s->incomingMigration,&s->incomingCheckpoint,error)) return 0;
 *found=s->incomingMigration!=NULL; return 1;
}
int xideSessionRebuild(xide_app* app,xide_recovery_guard* guard,xide_model_error* error)
{
    xide_session* s=app ? app->session : NULL;
    if(!s || !guard || !guard->lockHandle || !guard->directory || !xidePathEqual(s->directory,guard->directory)) return fail(error,XIDE_MODEL_INVALID,"Rebuild requires the backed-up scope's checkpoint lock.");
    if(!configure(app) || !s->ready || !s->layoutReady || !xidePathEqual(s->directory,guard->directory)) return fail(error,XIDE_MODEL_INVALID,"Cannot open fresh state after backup; complete original copies are preserved.");
    int published=0; int okay=checkpointLocked(app,0,error,&published);
    if(published) { s->issue[0]=0; if(app->statusBar) xuiStatusBarSetItemText(app->statusBar,1,"C / xlang · UTF-8"); }
    if(!okay) report(app,error ? error->message : "Rebuild failed; complete original copies are preserved.");
    /* Index failure is reported separately; a published main checkpoint is repaired. */
    return published;
}
char* xideSessionGlobalPath(xide_app* app,int kind)
{
    if(!app || kind<0 || kind>=XIDE_GLOBAL_COUNT) return NULL;
    if(kind==XIDE_GLOBAL_SETTINGS) {
        if(app->preferences.path) return copy(app->preferences.path);
        return app->settingsPath ? xidePathResolveLexical(NULL,app->settingsPath) : app->hidden ? NULL : xideSettingsDefaultPath();
    }
    xide_session* s=app->session; return s && s->globalDirectory ? xidePathResolveLexical(s->globalDirectory,kind==XIDE_GLOBAL_INDEX ? "recent.json" : "window.json") : NULL;
}
int xideSessionGlobalPublish(xide_app* app,int kind,xide_state_repair* repair,xide_model_error* error)
{
    char* path=xideSessionGlobalPath(app,kind); xide_session* s=app ? app->session : NULL;
    if(!path || !repair || !repair->path || !xidePathLexicalEqual(path,repair->path)) { free(path); return fail(error,XIDE_MODEL_CONFLICT,"Repair target no longer matches the selected global file."); }
    xvalue* root=kind==XIDE_GLOBAL_INDEX ? captureIndex(app) : kind==XIDE_GLOBAL_WINDOW ? xideWindowStateCaptureRoot(&s->window,xgePlatformNativeHandle(),error) :
        xideSettingsCapture(&app->preferences,path,app->codeFontPath,app->codeFontSize,app->uiFontPath,app->uiFontSize);
    int okay=root && xideStateRepairPublish(repair,root,error); xrtValueRelease(root);
    if(!okay && error && !error->message[0]) fail(error,XIDE_MODEL_MEMORY,"Cannot capture current global settings; original preserved.");
    if(okay) {
        if(kind==XIDE_GLOBAL_INDEX) { s->indexReady=xideStateOpen(&s->index,path,"xlang-ide-session-index-1",error); okay=s->indexReady; }
        else if(kind==XIDE_GLOBAL_WINDOW) okay=xideWindowStateOpen(&s->window,xgePlatformNativeHandle(),s->globalDirectory,0,xgeRenderRequest,error);
        else okay=xideSettingsLoad(&app->preferences,path,error);
        if(s && okay) {
            s->blocked=0;
            if(s->ready && s->layoutReady && s->indexReady && (!s->window.window || s->window.ready) && app->preferences.source) {
                s->issue[0]=0; if(app->statusBar) xuiStatusBarSetItemText(app->statusBar,1,"C / xlang · UTF-8");
            }
        }
    }
    free(path); return okay;
}
static xide_project* findProject(xide_app* app,const xvalue* entry)
{
    xide_project* p=xideWorkspaceFindProject(&app->workspaceModel,string(entry,"id","")); if(p) return p;
    char* path=xidePathResolve(app->session->base,string(entry,"manifest",""));
    for(size_t i=0;path && i<app->workspaceModel.projectCount;++i) if(xidePathEqual(path,app->workspaceModel.projects[i].manifest)) { free(path); return &app->workspaceModel.projects[i]; }
    free(path); return NULL;
}
static int restoreDocument(xide_app* app,const xvalue* entry)
{
    xide_session* s=app->session; const char* relative=string(entry,"path",""); char* path=*relative ? xidePathResolve(s->base,relative) : NULL; xide_model_error error={0}; xide_document* d=NULL;
    if(path && xideDocumentsFind(&app->documents,path)) { free(path); return 1; }
    const char* language=string(entry,"language",path ? xideLanguageForPath(path) : "text");
    if(get(entry,"recoveryText")) {
        char* bodyPath=xideRecoveryPath(s->directory,string(entry,"recoveryText",""),&error),*basePath=xideRecoveryPath(s->directory,string(entry,"recoveryBaseline",""),&error); xide_text_file body={0},baseline={0};
        if(bodyPath && basePath && xideTextFileRead(bodyPath,&body,&error) && xideTextFileRead(basePath,&baseline,&error))
            d=xideDocumentRecover(&app->documents,path,string(entry,"title",path ? xidePathFilename(path) : "恢复的未命名文件"),language,&baseline,body.text,&error);
        free(bodyPath); free(basePath); xideTextFileClear(&body); xideTextFileClear(&baseline);
    } else if(path) d=xideDocumentOpen(&app->documents,path,&error);
    else fail(&error,XIDE_MODEL_INVALID,"Untitled session document has no recovery text.");
    free(path);
    if(!d) { char message[512]; snprintf(message,sizeof(message),"无法恢复文件 %s：%.240s；会话引用和恢复文件已保留。",relative,error.message); report(app,message); xrtValueArrayAppendNew(s->unavailable,xrtValueDeepClone(entry)); return 0; }
    const char* id=text(get(entry,"id")); if(id) { char* owned=copy(id); if(owned) { free(d->sessionId); d->sessionId=owned; } }
    if(strcmp(d->language,language)) xideDocumentSetLanguage(d,language,&error);
    bool override=false; xrtValueGetBool(get(entry,"languageOverride"),&override); d->languageOverride=override;
    const xvalue* context=get(entry,"cAnalysisContext");
    if(context) { xvalue* cloned=xrtValueDeepClone(context); if(!cloned) { report(app,"无法恢复 C 项目上下文；会话引用已保留。"); xrtValueArrayAppendNew(s->unavailable,xrtValueDeepClone(entry)); return 0; } xrtValueRelease(d->cAnalysisContext); d->cAnalysisContext=cloned; }
    if(xideEditorShow(app,d)!=XUI_OK) return 0;
    const char* content=xuiCodeEditGetText(d->editor);
    int caret=xidePositionToOffset(content,integer(get(entry,"line"),1),integer(get(entry,"column"),1)),anchor=xidePositionToOffset(content,integer(get(entry,"anchorLine"),integer(get(entry,"line"),1)),integer(get(entry,"anchorColumn"),integer(get(entry,"column"),1)));
    xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),anchor,caret);
    const xvalue* collapsed=get(entry,"collapsedFolds"); xui_code_fold_state folds=xuiCodeEditGetFoldState(d->editor);
    for(int i=0;i<xuiCodeFoldStateGetCount(folds);++i) {
        xui_code_fold_range_t range={0}; range.iSize=sizeof(range); xuiCodeFoldStateGetRange(folds,i,&range);
        for(size_t j=0;j<xrtValueCount(collapsed);++j) {
            const xvalue* fold=xrtValueArrayGet(collapsed,j);
            if(range.iStartLine+1==integer(get(fold,"line"),0) && range.iEndLine+1==integer(get(fold,"endLine"),0) && !(range.iFlags & XUI_CODE_FOLD_COLLAPSED)) { xuiCodeFoldStateToggleLine(folds,range.iStartLine); break; }
        }
    }
    return 1;
}
/* Translate into independent native entries before touching documents or trees. */
static int legacyStatusValid(const xvalue* status)
{
 if(!status) return 1;
 if(!xrtValueIs(status,XVALUE_OBJECT)) return 0;
 const char* fields[]={"IsExpand","ProjExpand","FileExpand","LibsExpand","ResourceExpand"};
 for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i) { bool b=false; if(get(status,fields[i]) && !xrtValueGetBool(get(status,fields[i]),&b)) return 0; }
 return 1;
}
static int legacyFiles(xide_app* a,const xvalue* hints,const xide_files* files,const char* base,xvalue* entries,xide_model_error* e)
{
 if(!xrtValueIs(hints,XVALUE_ARRAY)) return fail(e,XIDE_MODEL_INVALID,"旧会话文件提示类型无效。");
 for(size_t i=0;i<xrtValueCount(hints);++i) {
  const xvalue* item=xrtValueArrayGet(hints,i); const char *id=text(get(item,"id")),*ref=text(get(item,"path")); bool open=false;
  if(!xrtValueIs(item,XVALUE_OBJECT) || !id || !*id || !ref || !*ref || !xrtValueGetBool(get(item,"open"),&open)) return fail(e,XIDE_MODEL_INVALID,"旧会话文件标识、路径或打开状态无效。");
  const xide_file* file=NULL; for(size_t j=0;j<files->count;++j) if(!strcmp(files->items[j].id,id)) { file=&files->items[j]; break; }
  char* path=xidePathResolve(base,ref); int same=file && path && xidePathEqual(file->path,path);
  if(!same) { free(path); return fail(e,XIDE_MODEL_CONFLICT,"旧会话文件提示与当前导入清单不一致。"); }
  if(!open) { free(path); continue; }
  int duplicate=0; for(size_t j=0;j<xrtValueCount(entries);++j) if(xidePathEqual(path,string(xrtValueArrayGet(entries,j),"path",""))) { duplicate=1; break; }
  if(duplicate) { free(path); continue; }
  if(xrtValueCount(entries)>=10000) { free(path); return fail(e,XIDE_MODEL_INVALID,"旧打开文件超过当前 10000 项会话限制。"); }
  xvalue* entry=xrtValueObject(); char* nativeId=xideCreateId("document");
  int okay=entry && nativeId && setText(entry,"id",nativeId) && setText(entry,"path",path) && setText(entry,"language",xideLanguageForPath(path)) && set(entry,"line",xrtValueInt(1)) && set(entry,"column",xrtValueInt(1));
  free(nativeId); free(path);
  if(okay) okay=xrtValueArrayAppendNew(entries,entry); else xrtValueRelease(entry);
  if(!okay) return fail(e,XIDE_MODEL_MEMORY,"无法准备导入文件会话。");
 }
 (void)a; return 1;
}
static const xvalue* legacyProject(const xvalue* hints,const char* id)
{
 const xvalue* projects=get(hints,"projects");
 for(size_t i=0;i<xrtValueCount(projects);++i) { const xvalue* p=xrtValueArrayGet(projects,i); if(!strcmp(id,string(p,"id",""))) return p; }
 return NULL;
}
static int legacyTree(xvalue* reading,const xvalue* hints)
{
 xvalue* nodes=get(get(reading,"workspace"),"nodes");
 for(size_t i=0;i<xrtValueCount(nodes);++i) {
  xvalue* node=xrtValueArrayGet(nodes,i); const char* id=string(node,"id",""); xvalue* key=xrtJsonParse(sv(id));
  const char *kind="",*project="",*group="";
  if(key && xrtValueIs(key,XVALUE_ARRAY) && xrtValueCount(key)==3) { kind=text(xrtValueArrayGet(key,0)); project=text(xrtValueArrayGet(key,1)); group=text(xrtValueArrayGet(key,2)); }
  const xvalue* status=NULL; const char* field=NULL;
  if(kind && project && group) {
   if(!strcmp(kind,"workspace")) { status=get(hints,"status"); field="IsExpand"; }
   else { status=get(legacyProject(hints,project),"status");
    if(!strcmp(kind,"project")) field="ProjExpand";
    else if(!strcmp(kind,"group")) {
     if(!strcmp(group,"libraries")) field="LibsExpand";
     else if(!strcmp(group,"group-3")) field="ResourceExpand";
     else if(!strcmp(group,"group-0") || !strcmp(group,"group-1") || !strcmp(group,"group-2") || !strcmp(group,"group-4")) field="FileExpand";
    }
   }
  }
  bool expanded=false; int okay=!field || !get(status,field) || (xrtValueGetBool(get(status,field),&expanded) && set(node,"expanded",xrtValueBool(expanded)));
  xrtValueRelease(key); if(!okay) return 0;
 }
 return xideReadingValid(reading);
}
int xideSessionApplyLegacyHints(xide_app* a,const xvalue* hints,xide_legacy_session_result* result,xide_model_error* e)
{
 xide_legacy_session_result local={0}; if(!result) result=&local; memset(result,0,sizeof(*result));
 if(e) memset(e,0,sizeof(*e));
 xide_session* s=a ? a->session : NULL;
 if(!a || !a->workspaceModel.manifest || !xrtValueIs(hints,XVALUE_OBJECT) || strcmp(string(hints,"format",""),"xlang-ide-legacy-session-hints-1") || !xidePathEqual(a->workspaceModel.manifest,string(hints,"workspace",""))) return fail(e,XIDE_MODEL_INVALID,"旧会话提示不属于当前导入的工作空间。");
 if(s && (!s->ready || !s->layoutReady || s->metadata.existed || s->layout.existed || s->legacyHints)) return fail(e,XIDE_MODEL_CONFLICT,"已有或受保护的原生会话不会被旧状态覆盖。");
 const xvalue* projects=get(hints,"projects");
 if(!xrtValueIs(projects,XVALUE_ARRAY) || xrtValueCount(projects)!=a->workspaceModel.projectCount || !legacyStatusValid(get(hints,"status"))) return fail(e,XIDE_MODEL_INVALID,"旧会话项目或展开状态无效。");
 char* base=xidePathDirectory(a->workspaceModel.manifest); xvalue *entries=xrtValueArray(),*reading=NULL,*raw=xrtValueDeepClone(hints);
 int okay=base && entries && raw;
 for(size_t i=0;okay && i<xrtValueCount(projects);++i) {
  const xvalue* item=xrtValueArrayGet(projects,i); const char *id=text(get(item,"id")),*manifest=text(get(item,"manifest")); bool open=false,supported=false;
  xide_project* p=id ? xideWorkspaceFindProject(&a->workspaceModel,id) : NULL;
  okay=p && manifest && xidePathEqual(p->manifest,manifest) && xrtValueGetBool(get(item,"open"),&open) && xrtValueGetBool(get(item,"supported"),&supported) && legacyStatusValid(get(item,"status"));
  for(size_t j=0;okay && j<i;++j) if(!strcmp(id,string(xrtValueArrayGet(projects,j),"id",""))) okay=0;
  if(okay) okay=legacyFiles(a,get(item,"files"),&p->files,base,entries,e);
 }
 if(okay) okay=legacyFiles(a,get(hints,"files"),&a->workspaceModel.files,base,entries,e);
 if(okay && s) for(size_t i=0;okay && i<xrtValueCount(entries);++i) {
  xvalue* entry=xrtValueArrayGet(entries,i); char* relative=xidePathRelative(s->base,string(entry,"path",""));
  okay=relative && setText(entry,"path",relative); free(relative);
 }
 if(okay) { reading=xideReadingCapture(a,NULL); okay=reading && legacyTree(reading,hints); }
 if(!okay) { free(base); xrtValueRelease(entries); xrtValueRelease(reading); xrtValueRelease(raw); if(!e || !e->code) fail(e,XIDE_MODEL_INVALID,"旧会话提示与当前清单不匹配，当前状态保留。"); return 0; }
 if(s) s->restoring=1;
 for(size_t i=0;i<xrtValueCount(entries);++i) {
  xvalue* entry=xrtValueArrayGet(entries,i); const char* path=string(entry,"path","");
  int opened=s ? restoreDocument(a,entry) : xideEditorOpen(a,path,NULL);
  if(opened) ++result->opened; else ++result->unavailable;
 }
 xideReadingRestore(a,reading,0); xideWorkbenchLayout(a); if(xuiLayout(a->context)==XUI_OK) xideReadingRestore(a,reading,1);
 result->applied=1;
 if(s) { s->legacyHints=raw; raw=NULL; s->restoring=0; }
 free(base); xrtValueRelease(entries); xrtValueRelease(reading); xrtValueRelease(raw);
 if(s) { if(!xideSessionSave(a,0,e)) return 0; result->persisted=1; }
 xideActionsRefresh(a); xgeRenderRequest(); return 1;
}

static void restore(xide_app* app)
{
    xide_session* s=app->session; xvalue* recovered=NULL; xide_model_error journalError={0};
    const xvalue* paired=s->ready && s->metadata.existed && s->layoutReady && layoutState(s,string(s->metadata.source,"layoutGeneration","")) ? s->metadata.source : NULL;
    if(s->incomingMigration) {
        s->migrationJournal=s->incomingMigration; s->incomingMigration=NULL;
        if(xideMigrationJournalStage(s->migrationJournal,s->directory,s->base,paired,&s->incomingCheckpoint,&journalError)) { s->restoredCheckpoint=s->incomingCheckpoint; s->incomingCheckpoint=NULL; }
        else { report(app,journalError.message); xrtValueRelease(s->incomingCheckpoint); s->incomingCheckpoint=NULL; }
    }
    if(!xideRenameJournalRecover(app,s->directory,s->base,paired,&s->renameJournal,&recovered,&journalError)) report(app,journalError.message);
    if(!recovered && !s->restoredCheckpoint && (!s->ready || !s->metadata.existed)) return;
    s->restoring=1; const xvalue* root=recovered ? recovered : s->restoredCheckpoint ? s->restoredCheckpoint : s->metadata.source;
    xvalue* docs=get(root,"openDocuments");
    for(size_t i=0;i<xrtValueCount(docs);++i) restoreDocument(app,xrtValueArrayGet(docs,i));
    if(recovered && !xideRenameJournalApplyRecovered(s->renameJournal,app,&journalError)) report(app,journalError.message);
    xide_document* active=NULL; const char* id=string(root,"activeDocumentId","");
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && !strcmp(app->documents.items[i]->sessionId,id)) active=app->documents.items[i];
    if(!active && *string(root,"activeDocument","")) { char* path=xidePathResolve(s->base,string(root,"activeDocument","")); active=path ? xideDocumentsFind(&app->documents,path) : NULL; free(path); }
    if(active) xideEditorShow(app,active);
    const char* project=string(root,"activeProject",""); if(*project) xideWorkspaceViewSelectProject(app,project);
    xvalue* configs=get(root,"configurations");
    for(size_t i=0;i<xrtValueCount(configs);++i) { xide_project* p=findProject(app,xrtValueArrayGet(configs,i)); const char* config=string(xrtValueArrayGet(configs,i),"configuration",""); if(p && xideProjectFindConfiguration(p,config)) { char* value=copy(config); if(value) { free(p->activeConfiguration); p->activeConfiguration=value; } } }
    if(*project && !xideWorkspaceFindProject(&app->workspaceModel,project) && xrtValueCount(configs)) { xide_project* p=findProject(app,xrtValueArrayGet(configs,0)); if(p) xideWorkspaceViewSelectProject(app,p->id); }
    xideWorkspaceViewRefresh(app);
    if(!app->themeExplicit) { int theme=ideThemeFind(string(root,"theme","")); if(theme>=0) xideWorkbenchApplyTheme(app,theme); }
    const char* query=string(root,"commandSearch",""); xideCatalogViewSearch(app,query);
    const xide_command* command=xideCatalogFind(&app->catalog,string(root,"selectedCommand","")); if(command) xideWorkbenchShowHelp(app,(int)(command-app->catalog.entries));
    xideReadingRestore(app,get(root,"reading"),0);
    const xvalue* layout=s->layoutReady ? layoutState(s,string(root,"layoutGeneration","")) : NULL;
    const xvalue* dock=layout ? get(layout,"dock") : xideMigrationJournalDock(s->migrationJournal,string(root,"layoutGeneration",""));
    xide_model_error error={0}; if(dock && !xideLayoutRestore(app,dock,&error)) { s->layoutReady=0; report(app,error.message); }
    if(active) {
        app->activeDocument=active; xui_dock_window_info_t info={0}; info.iSize=sizeof(info); xuiDockPanelGetWindowInfo(app->dock,active->window,&info);
        if(info.iState==XUI_DOCK_PANEL_WINDOW_HIDDEN) xideEditorShow(app,active);
        else xuiSetFocusWidget(app->context,active->editor);
    } else if(!app->activeDocument) {
        xui_dock_window_info_t xlang={0},c={0}; xlang.iSize=sizeof(xlang); c.iSize=sizeof(c);
        xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_XLANG_DEMO],&xlang); xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_C_DEMO],&c);
        if(xlang.iState==XUI_DOCK_PANEL_WINDOW_HIDDEN && c.iState==XUI_DOCK_PANEL_WINDOW_HIDDEN) xideDispatchAction(app,XIDE_ACTION_XLANG);
    }
    s->scrollPending=1; s->restoring=0; xrtValueRelease(recovered);
}
int xideSessionStart(xide_app* app)
{
    if(app->session || app->noSession || (app->hidden && !app->sessionRoot)) return 1;
    xide_session* s=calloc(1,sizeof(*s)); if(!s) return 0; app->session=s; s->restoring=1;
    s->globalDirectory=globalDirectory(app); if(!s->globalDirectory) { report(app,"无法定位会话目录；本次会话持久化不可用。"); return 1; }
    char* path=xidePathResolve(s->globalDirectory,"recent.json"); xide_model_error error={0};
    if(!app->hidden && xgePlatformNativeHandle() && !xideWindowStateOpen(&s->window,xgePlatformNativeHandle(),s->globalDirectory,!app->windowSizeExplicit,xgeRenderRequest,&error)) report(app,error.message);
    s->indexReady=path && xideStateOpen(&s->index,path,"xlang-ide-session-index-1",&error); free(path); if(!s->indexReady) report(app,error.message);
    if(s->indexReady && s->index.existed && !validIndex(s->index.source)) { s->indexReady=0; report(app,"Recent-project index is invalid; the original is preserved."); }
    if(app->workspacePath || app->projectPath) {
        if(!xideWorkspaceOpen(app,app->workspacePath ? app->workspacePath : app->projectPath,app->projectPath!=NULL,&error)) { xideFailureSet(&app->failure,XIDE_FAILURE_WORKSPACE,"model",error.code,error.message,"xideWorkspaceOpen"); report(app,error.message); return 0; }
    }
    if(!owner(app) && !app->workspacePath && !app->projectPath && s->indexReady && s->index.existed) {
        const char* kind=string(s->index.source,"kind","none"),*last=string(s->index.source,"path","");
        if(*last && (!strcmp(kind,"workspace") || !strcmp(kind,"project")) && !xideWorkspaceOpen(app,last,!strcmp(kind,"project"),&error)) report(app,error.message);
    }
    if(!owner(app) && !app->workspacePath && !app->projectPath && !s->incomingMigration) {
        xide_workspace model={0}; char* projectPath=NULL; int found=0;
        if(!xideSessionLoadMigration(app,"",0,&model,&projectPath,&found,&error)) report(app,error.message);
        else if(found) { xideWorkspaceClear(&app->workspaceModel); app->workspaceModel=model; memset(&model,0,sizeof(model)); xideWorkspaceViewRefresh(app); }
        xideWorkspaceClear(&model); free(projectPath);
    }
    s->restoring=0; if(!configure(app)) return 0; restore(app); s->fingerprint=current(app); return 1;
}
int xideSessionCanAttemptCheckpoint(xide_app* app)
{ return app && app->session && !app->session->restoring; }
int xideSessionBeforeWorkspace(xide_app* app,xide_model_error* error)
{
    xide_session* s=app->session; if((!s && !app->sessionSwitchConfirmed) || (s && s->restoring)) return 1;
    if(s && !xideSessionSave(app,app->sessionSwitchConfirmed,error)) {
        report(app,error ? error->message : "Cannot checkpoint session."); if(xideDocumentsDirty(&app->documents) && !app->sessionSwitchConfirmed) return 0;
    }
    if(s) s->restoring=1;
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open) {
        if(!xideEditorClose(app,app->documents.items[i],1,error)) { if(s) s->restoring=0; return 0; }
    }
    xideWorkbenchReset(app); xideReadingReset(app);
    if(s) s->restoring=0;
    return 1;
}
void xideSessionAfterWorkspace(xide_app* app)
{
    if(!app->session || app->session->restoring) return;
    if(configure(app)) restore(app); else report(app,"会话目录初始化失败。");
    app->session->fingerprint=current(app); app->session->due=0;
}
void xideSessionRelocate(xide_app* app)
{
    xide_session* s=app->session; if(!s || s->restoring) return;
    xvalue* metadata=xrtValueDeepClone(s->metadata.source),*layout=xrtValueDeepClone(s->layout.source);
    xide_migration_journal* migration=s->migrationJournal; s->migrationJournal=NULL;
    if(configure(app)) {
        s->migrationJournal=migration; migration=NULL;
        if(s->ready && !s->metadata.existed && metadata) { xrtValueRelease(s->metadata.source); s->metadata.source=metadata; metadata=NULL; }
        if(s->layoutReady && !s->layout.existed && layout) { xrtValueRelease(s->layout.source); s->layout.source=layout; layout=NULL; }
        xide_model_error error={0}; if(!xideSessionSave(app,0,&error)) report(app,error.message);
    }
    xideRenameJournalDestroy(migration); xrtValueRelease(metadata); xrtValueRelease(layout);
}
void xideSessionPoll(xide_app* app)
{
    xide_session* s=app->session; if(!s) return;
    if(xideRecoveryViewBusy(app)) return;
    xide_model_error windowError={0}; if(!xideWindowStatePoll(&s->window,&windowError)) report(app,windowError.message);
    if(!s->ready || s->restoring || s->blocked || xideRecoveryViewBusy(app)) return;
    uint64_t fingerprint=current(app),now=GetTickCount64();
    if(!s->migrationMaintenance) s->migrationMaintenance=xideRecoveryServiceCreate(NULL);
    if(s->migrationMaintenance) {
        xide_recovery_result result={0};
        if(xideRecoveryServiceTake(s->migrationMaintenance,&result)) { if(!result.okay) fprintf(stderr,"migration maintenance: %s\n",result.error.message); xideRecoveryResultClear(&result,0); }
        if(!xideRecoveryServiceBusy(s->migrationMaintenance) && (!s->lastMigrationMaintenance || now-s->lastMigrationMaintenance>=UINT64_C(3600000))) {
            xide_model_error maintenance={0}; s->lastMigrationMaintenance=now ? now : 1;
            if(!xideRecoveryServiceStart(s->migrationMaintenance,XIDE_RECOVERY_MIGRATIONS,s->globalDirectory,&maintenance)) fprintf(stderr,"migration maintenance: %s\n",maintenance.message);
        }
        if(xideRecoveryServiceBusy(s->migrationMaintenance)) xgeRenderRequest();
    }
    if(fingerprint!=s->fingerprint) { s->fingerprint=fingerprint; s->due=now+750; }
    if(s->due && now>=s->due) { xide_model_error error={0}; if(!xideSessionSave(app,0,&error)) { s->blocked=1; report(app,error.message); } }
    if(s->due) xgeRenderRequest();
}
void xideSessionAfterLayout(xide_app* app)
{
    xide_session* s=app->session; if(!s || !s->scrollPending) return; s->scrollPending=0;
    xideReadingRestore(app,get(s->restoredCheckpoint ? s->restoredCheckpoint : s->metadata.source,"reading"),1);
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open) {
        xide_document* d=app->documents.items[i]; const xvalue* entry=oldEntry(s,d->sessionId); double x=0,y=0;
        if(numeric(get(entry,"scrollX"),&x) && numeric(get(entry,"scrollY"),&y) && isfinite(x) && isfinite(y) && x>=0 && y>=0 && x<=10000000 && y<=10000000)
            xuiCodeEditSetScroll(d->editor,(float)x,(float)y);
        else xuiCodeEditEnsureCaretVisible(d->editor);
    }
}
void xideSessionDestroy(xide_app* app)
{
    xide_session* s=app->session; if(!s) return; xideRecoveryServiceDestroy(s->migrationMaintenance); xideSessionDiscardMigration(app); xideWindowStateClose(&s->window); clearScope(s); xrtValueRelease(s->unavailable); xideStateClear(&s->index); free(s->globalDirectory); free(s); app->session=NULL;
}
const char* xideSessionDirectory(xide_app* app) { return app && app->session ? app->session->directory : NULL; }
const char* xideSessionIssue(xide_app* app) { return app && app->session ? app->session->issue : ""; }
