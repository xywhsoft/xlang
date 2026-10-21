#include "editor/documents.h"
#include "platform/dpi_proxy.h"
#include "platform/file_rename.h"
#include "editor/coordinates.h"
#include "platform/path.h"
#include "lang/languages.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static char* copy(const char* text)
{
    size_t length=strlen(text)+1; char* result=malloc(length);
    if(result) memcpy(result,text,length);
    return result;
}
static int failure(xide_model_error* error,int code,const char* message)
{
    if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); }
    return 0;
}
static void changed(xui_code_document core,xui_code_range_t range,uint32_t version,void* user)
{
    (void)range; (void)version; xide_document* document=user;
    if(!document->open || document->suppressChange) return;
    const char* text=xuiCodeDocumentGetText(core);
    document->dirty=!document->baseline.text || strcmp(text,document->baseline.text)!=0;
    xuiCodeDocumentSetDirty(core,document->dirty);
    if(xideRefreshEditorSyntax(document->editor)!=XUI_OK) fputs("Document syntax refresh failed.\n",stderr);
    if(document->store->onChanged) document->store->onChanged(document,document->store->user);
}
void xideDocumentsInit(xide_document_store* store,xui_context context,xui_font font,xide_document_changed_proc onChanged,void* user)
{
    memset(store,0,sizeof(*store)); store->context=context; store->font=font; store->onChanged=onChanged; store->user=user;
}
static void clearMetadata(xide_document* document)
{
    xrtValueRelease(document->cAnalysisContext); document->cAnalysisContext=NULL;
    xideTaskStageRelease(document->stageReference); document->stageReference=NULL;
    free(document->path); free(document->title); free(document->language); free(document->sessionId);
    document->path=document->title=document->language=document->sessionId=NULL; xideTextFileClear(&document->baseline);
    document->dirty=0; document->open=0; document->languageOverride=0; document->externalState=0;
}
int xideDocumentCContextValid(const xvalue* context)
{
    if(!context) return 1;
    if(!xrtValueIs(context,XVALUE_OBJECT)) return 0;
    const char* keys[]={"project","source","sourceId"};
    for(size_t i=0;i<3;++i) {
        const xvalue* field=xrtValueObjectGet(context,(xstrview){keys[i],strlen(keys[i])});
        if(i==2 && !field) continue;
        xstrview text={0};
        if(!xrtValueGetString(field,&text) || !text.Size || text.Size>32768 || memchr(text.Data,0,text.Size) ||
           !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.Data,(int)text.Size,NULL,0)) return 0;
    }
    size_t size=0; char* encoded=xrtJsonStringify(context,false,&size);
    int valid=encoded && size<=128u*1024u; xrtFree(encoded); return valid;
}
void xideDocumentsClear(xide_document_store* store)
{
    for(size_t i=0;i<store->count;++i) {
        xide_document* document=store->items[i];
        xuiCodeDocumentSetChangeCallback(xuiCodeEditGetDocument(document->editor),NULL,NULL);
        clearMetadata(document); free(document);
    }
    free(store->items); memset(store,0,sizeof(*store));
}
xide_document* xideDocumentsFind(xide_document_store* store,const char* path)
{
    if(!path) return NULL;
    for(size_t i=0;i<store->count;++i) if(store->items[i]->open && store->items[i]->path && xidePathEqual(store->items[i]->path,path)) return store->items[i];
    return NULL;
}
static xide_document* acquire(xide_document_store* store,xide_model_error* error)
{
    for(size_t i=0;i<store->count;++i) if(!store->items[i]->open) return store->items[i];
    if(store->count==store->capacity) {
        size_t capacity=store->capacity ? store->capacity*2 : 8;
        if(capacity<store->capacity || capacity>SIZE_MAX/sizeof(*store->items)) { failure(error,XIDE_MODEL_MEMORY,"Document capacity overflow."); return NULL; }
        xide_document** items=realloc(store->items,capacity*sizeof(*items));
        if(!items) { failure(error,XIDE_MODEL_MEMORY,"Document allocation failed."); return NULL; }
        store->items=items; store->capacity=capacity;
    }
    xide_document* document=calloc(1,sizeof(*document));
    if(!document) { failure(error,XIDE_MODEL_MEMORY,"Document allocation failed."); return NULL; }
    document->store=store; document->window=-1;
    xui_code_edit_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=store->font;
    desc.bShowLineNumbers=1; desc.bShowFoldMargin=1; desc.iTabColumns=4; desc.iIndentColumns=4;
    int result=xideDpiCodeEditCreate(store->context,&document->editor,&desc);
    if(result==XUI_OK) result=xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(document->editor));
    if(result!=XUI_OK) {
        if(document->editor) xuiWidgetDestroy(document->editor);
        free(document); failure(error,XIDE_MODEL_MEMORY,"CodeEdit creation failed."); return NULL;
    }
    store->items[store->count++]=document;
    xuiCodeDocumentSetChangeCallback(xuiCodeEditGetDocument(document->editor),changed,document);
    return document;
}
static int initialize(xide_document* document,char* path,const char* title,const char* language,xide_text_file* file,xide_model_error* error)
{
    char* name=copy(title),*mode=copy(language),*sessionId=xideCreateId("doc");
    if(!name || !mode || !sessionId) { free(name); free(mode); free(sessionId); return failure(error,XIDE_MODEL_MEMORY,"Document metadata allocation failed."); }
    document->suppressChange=1;
    int result=xuiCodeEditSetText(document->editor,file->text);
    if(result==XUI_OK) result=xideConfigureEditorLanguage(document->editor,language);
    document->suppressChange=0;
    if(result!=XUI_OK) { free(name); free(mode); free(sessionId); return failure(error,XIDE_MODEL_MEMORY,"Document initialization failed."); }
    clearMetadata(document); document->path=path; document->title=name; document->language=mode; document->sessionId=sessionId;
    document->baseline=*file; memset(file,0,sizeof(*file)); document->id=++document->store->nextId; document->open=1;
    xuiCodeDocumentSetDirty(xuiCodeEditGetDocument(document->editor),0);
    xuiCodeEditSetEolMode(document->editor,document->baseline.eol);
    xuiCodeSelectionSetRange(xuiCodeEditGetSelection(document->editor),xuiCodeEditGetDocument(document->editor),0,0);
    xuiCodeEditSetScroll(document->editor,0,0);
    return 1;
}
xide_document* xideDocumentOpen(xide_document_store* store,const char* path,xide_model_error* error)
{
    char* resolved=xidePathResolve(NULL,path);
    if(!resolved) { failure(error,XIDE_MODEL_INVALID,"Invalid document path."); return NULL; }
    xide_document* document=xideDocumentsFind(store,resolved);
    if(document) { free(resolved); return document; }
    xide_text_file file={0};
    xide_model_error protectionError={0}; xide_task_stage* reference=xideTaskStageProtectPath(resolved,&protectionError);
    if(protectionError.code) { if(error) *error=protectionError; free(resolved); return NULL; }
    if(!xideTextFileRead(resolved,&file,error)) { xideTaskStageRelease(reference); free(resolved); return NULL; }
    document=acquire(store,error);
    if(!document || !initialize(document,resolved,xidePathFilename(resolved),xideLanguageForPath(resolved),&file,error)) {
        xideTaskStageRelease(reference); free(resolved); xideTextFileClear(&file); return NULL;
    }
    document->stageReference=reference;
    return document;
}
xide_document* xideDocumentNew(xide_document_store* store,const char* language,xide_model_error* error)
{
    if(!language || (strcmp(language,"c") && strcmp(language,"xlang") && strcmp(language,"text"))) {
        failure(error,XIDE_MODEL_INVALID,"Unsupported document language."); return NULL;
    }
    xide_document* document=acquire(store,error); xide_text_file file={0}; char title[64];
    snprintf(title,sizeof(title),"未命名 %llu",(unsigned long long)(store->nextId+1));
    if(!document || !xideTextFileEncode("",0,0,XUI_CODE_EOL_LF,&file,error) || !initialize(document,NULL,title,language,&file,error)) { xideTextFileClear(&file); return NULL; }
    return document;
}
int xideDocumentCheckExternal(xide_document* document)
{
    if(!document || !document->open || !document->path) return 0;
    unsigned char* data=NULL; size_t size=0;
    data=xrtFileReadAllLimit(document->path,32u*1024u*1024u,&size);
    if(!data) return 1;
    int result=size!=document->baseline.rawLength || (size && memcmp(data,document->baseline.raw,size));
    xrtFree(data); return result!=0;
}
static int saveFinished(xide_document* document,int saved)
{ if(document->store->saveFinished) document->store->saveFinished(document,saved,document->store->user); return saved; }
int xideDocumentSave(xide_document* document,const char* path,int flags,xide_model_error* error)
{
    if(!document || !document->open || (!path && !document->path)) return failure(error,XIDE_MODEL_INVALID,"Choose a path before saving this document.");
    char* target=xidePathResolve(NULL,path ? path : document->path);
    if(!target) return failure(error,XIDE_MODEL_MEMORY,"Save path allocation failed.");
    if(document->store->beforeSave && !document->store->beforeSave(document,target,error,document->store->user)) { free(target); return saveFinished(document,0); }
    xide_document* other=xideDocumentsFind(document->store,target);
    if(other && other!=document) { free(target); return saveFinished(document,failure(error,XIDE_MODEL_CONFLICT,"The save target is already open as another document.")); }
    int same=document->path && xidePathEqual(target,document->path);
    if((same && !(flags & XIDE_DOCUMENT_OVERWRITE_EXTERNAL) && xideDocumentCheckExternal(document)) ||
       (!same && xidePathExists(target) && !(flags & XIDE_DOCUMENT_OVERWRITE_TARGET))) {
        free(target); return saveFinished(document,failure(error,XIDE_MODEL_CONFLICT,"The file changed on disk or the save target already exists; confirm overwrite or reload first."));
    }
    if(same && !strcmp(target,document->path) && !document->dirty && !(flags & XIDE_DOCUMENT_OVERWRITE_EXTERNAL)) { free(target); return saveFinished(document,1); }
    char* title=copy(xidePathFilename(target)); char* language=copy(document->languageOverride ? document->language : xideLanguageForPath(target)); xide_text_file encoded={0};
    if(!title || !language || !xideTextFileEncode(xuiCodeEditGetText(document->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(document->editor)),document->baseline.bom,document->baseline.eol,&encoded,error)) {
        free(target); free(title); free(language); xideTextFileClear(&encoded); return saveFinished(document,0);
    }
    xide_model_error protectionError={0}; xide_task_stage* reference=xideTaskStageProtectPath(target,&protectionError);
    if(protectionError.code) {
        if(error) *error=protectionError;
        free(target); free(title); free(language); xideTextFileClear(&encoded); return saveFinished(document,0);
    }
    if(!xrtFileWriteAtomic(target,(xbytesview){encoded.raw,encoded.rawLength})) {
        xideTaskStageRelease(reference);
        free(target); free(title); free(language); xideTextFileClear(&encoded); return saveFinished(document,failure(error,XIDE_MODEL_IO,"Atomic document save failed; the original file and dirty state are retained."));
    }
    xideTaskStageRelease(document->stageReference); document->stageReference=reference;
    free(document->path); free(document->title); free(document->language); document->path=target; document->title=title; document->language=language;
    xideConfigureEditorLanguage(document->editor,language);
    xideTextFileClear(&document->baseline); document->baseline=encoded; document->dirty=0; document->externalState=0;
    xuiCodeDocumentSetDirty(xuiCodeEditGetDocument(document->editor),0);
    saveFinished(document,1);
    if(document->store->afterSave) document->store->afterSave(document,document->store->user);
    if(document->store->onChanged) document->store->onChanged(document,document->store->user);
    return 1;
}
int xideDocumentRenameSource(xide_document* document,const char* source,const char* path,const xide_file_snapshot* expected,xide_model_error* error)
{
    if(!document || !document->open || !document->path || !path) return failure(error,XIDE_MODEL_INVALID,"重命名需要已保存的打开文档。");
    if(!source || !xidePathEqual(source,document->path)) return failure(error,XIDE_MODEL_CONFLICT,"选中文件已不再指向当前编辑文档。");
    char* target=xidePathResolveLexical(NULL,path); if(!target) return failure(error,XIDE_MODEL_INVALID,"目标路径无效。");
    if(!strcmp(target,source)) { free(target); return 1; }
    if(document->store->beforeSave && !document->store->beforeSave(document,target,error,document->store->user)) { free(target); return saveFinished(document,0); }
    xide_document* other=xideDocumentsFind(document->store,target);
    if(other && other!=document) { free(target); return saveFinished(document,failure(error,XIDE_MODEL_CONFLICT,"目标作为另一个文档打开，请先处理目标文件。")); }
    char* title=copy(xidePathFilename(target)); char* language=copy(document->languageOverride ? document->language : xideLanguageForPath(target));
    if(!title || !language) { free(target); free(title); free(language); return saveFinished(document,failure(error,XIDE_MODEL_MEMORY,"无法准备文档重命名元数据。")); }
    xide_model_error protectionError={0}; xide_task_stage* reference=xideTaskStageProtectPath(target,&protectionError);
    if(protectionError.code) { if(error) *error=protectionError; free(target); free(title); free(language); return saveFinished(document,0); }
    if(!xideFileRenameChecked(source,target,&document->baseline,expected,error)) { xideTaskStageRelease(reference); free(target); free(title); free(language); return saveFinished(document,0); }
    xideTaskStageRelease(document->stageReference); document->stageReference=reference;
    if(document->store->pathChanged) { document->store->pathChanged(document,document->path,target,document->store->user); if(strcmp(source,document->path)) document->store->pathChanged(document,source,target,document->store->user); }
    int syntaxChanged=strcmp(document->language,language)!=0;
    free(document->path); free(document->title); free(document->language); document->path=target; document->title=title; document->language=language; document->externalState=0;
    if(syntaxChanged) { xideConfigureEditorLanguage(document->editor,language); } saveFinished(document,1);
    if(document->store->onChanged) { document->store->onChanged(document,document->store->user); } return 1;
}
int xideDocumentRename(xide_document* document,const char* path,xide_model_error* error)
{ return xideDocumentRenameSource(document,document ? document->path : NULL,path,NULL,error); }

typedef struct saved_selection { xui_code_selection_t state; int anchorLine,anchorColumn,caretLine,caretColumn; } saved_selection;
int xideDocumentAcceptExternal(xide_document* document,xide_text_file* loaded,int discardChanges,xide_model_error* error)
{
    if(!document || !document->open || !document->path || !loaded || !loaded->raw || !loaded->text) return failure(error,XIDE_MODEL_INVALID,"Invalid external document snapshot.");
    if(document->dirty && !discardChanges) return failure(error,XIDE_MODEL_CONFLICT,"Reload would discard unsaved changes.");
    xui_code_document core=xuiCodeEditGetDocument(document->editor); xui_code_selection_model selection=xuiCodeEditGetSelection(document->editor);
    int count=xuiCodeSelectionGetCount(selection); saved_selection* saved=count>0 ? calloc((size_t)count,sizeof(*saved)) : NULL;
    if(count>0 && !saved) return failure(error,XIDE_MODEL_MEMORY,"Cannot preserve document selections.");
    const char* before=xuiCodeDocumentGetText(core);
    for(int i=0;i<count;++i) {
        if(xuiCodeSelectionGetAt(selection,i,&saved[i].state)!=XUI_OK) { free(saved); return failure(error,XIDE_MODEL_INVALID,"Cannot capture document selection."); }
        xidePositionFromOffset(before,saved[i].state.iAnchorOffset,&saved[i].anchorLine,&saved[i].anchorColumn);
        xidePositionFromOffset(before,saved[i].state.iCaretOffset,&saved[i].caretLine,&saved[i].caretColumn);
    }
    float x=0,y=0; xuiCodeEditGetScroll(document->editor,&x,&y);
    document->suppressChange=1;
    int result=strcmp(before,loaded->text) ? xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),loaded->text) : XUI_OK;
    document->suppressChange=0;
    if(result!=XUI_OK) { free(saved); return failure(error,XIDE_MODEL_MEMORY,"Reload failed; the local text and disk baseline were retained."); }
    xideTextFileClear(&document->baseline); document->baseline=*loaded; memset(loaded,0,sizeof(*loaded)); document->dirty=0; document->externalState=0;
    xuiCodeDocumentSetDirty(core,0); xuiCodeEditSetEolMode(document->editor,document->baseline.eol); xideRefreshEditorSyntax(document->editor);
    const char* after=xuiCodeDocumentGetText(core); xuiCodeSelectionClear(selection);
    for(int i=0;i<count;++i) {
        saved[i].state.iAnchorOffset=xidePositionToOffset(after,saved[i].anchorLine,saved[i].anchorColumn);
        saved[i].state.iCaretOffset=xidePositionToOffset(after,saved[i].caretLine,saved[i].caretColumn);
        if(!i) xuiCodeSelectionSetState(selection,core,&saved[i].state); else xuiCodeSelectionAdd(selection,&saved[i].state);
    }
    free(saved); xuiCodeEditSetScroll(document->editor,x,y);
    if(document->store->onChanged) document->store->onChanged(document,document->store->user);
    return 1;
}
int xideDocumentReload(xide_document* document,int discardChanges,xide_model_error* error)
{
    if(!document || !document->open || !document->path) return failure(error,XIDE_MODEL_INVALID,"This document has no saved file.");
    if(document->dirty && !discardChanges) return failure(error,XIDE_MODEL_CONFLICT,"Reload would discard unsaved changes.");
    xide_text_file loaded={0};
    if(!xideTextFileRead(document->path,&loaded,error)) return 0;
    int result=xideDocumentAcceptExternal(document,&loaded,discardChanges,error); xideTextFileClear(&loaded); return result;
}
int xideDocumentClose(xide_document* document,int discardChanges,xide_model_error* error)
{
    if(!document || !document->open) return 1;
    if(document->dirty && !discardChanges) return failure(error,XIDE_MODEL_CONFLICT,"Closing would discard unsaved changes.");
    document->suppressChange=1; int result=xuiCodeEditSetText(document->editor,""); document->suppressChange=0;
    if(result!=XUI_OK) return failure(error,XIDE_MODEL_MEMORY,"Unable to release document text.");
    clearMetadata(document);
    if(document->store->onChanged) document->store->onChanged(document,document->store->user);
    return 1;
}
int xideDocumentSetLanguage(xide_document* document,const char* language,xide_model_error* error)
{
    if(!document || !document->open || !language || (strcmp(language,"c") && strcmp(language,"xlang") && strcmp(language,"text"))) return failure(error,XIDE_MODEL_INVALID,"Unsupported document language.");
    char* mode=copy(language); if(!mode) return failure(error,XIDE_MODEL_MEMORY,"Language allocation failed.");
    if(xideConfigureEditorLanguage(document->editor,language)!=XUI_OK) { free(mode); return failure(error,XIDE_MODEL_INVALID,"Unable to set document language."); }
    free(document->language); document->language=mode; document->languageOverride=1;
    if(document->store->onChanged) document->store->onChanged(document,document->store->user);
    return 1;
}
int xideDocumentsDirty(const xide_document_store* store)
{
    for(size_t i=0;i<store->count;++i) if(store->items[i]->open && store->items[i]->dirty) return 1;
    return 0;
}
xide_document* xideDocumentRecover(xide_document_store* store,const char* path,const char* title,const char* language,xide_text_file* baseline,const char* recovered,xide_model_error* error)
{
    if(!store || !title || !language || !baseline || !baseline->text || !recovered || (strcmp(language,"c") && strcmp(language,"xlang") && strcmp(language,"text"))) { failure(error,XIDE_MODEL_INVALID,"Invalid recovery document."); return NULL; }
    char* resolved=path && *path ? xidePathResolve(NULL,path) : NULL;
    if(path && *path && !resolved) { failure(error,XIDE_MODEL_INVALID,"Invalid recovery path."); return NULL; }
    if(resolved && xideDocumentsFind(store,resolved)) { free(resolved); failure(error,XIDE_MODEL_CONFLICT,"Recovered physical file is already open."); return NULL; }
    xide_model_error protectionError={0}; xide_task_stage* reference=xideTaskStageProtectPath(resolved,&protectionError);
    if(protectionError.code) { if(error) *error=protectionError; free(resolved); return NULL; }
    xide_document* document=acquire(store,error);
    if(!document || !initialize(document,resolved,title,language,baseline,error)) { xideTaskStageRelease(reference); free(resolved); return NULL; }
    document->stageReference=reference;
    xui_code_document core=xuiCodeEditGetDocument(document->editor);
    if(strcmp(recovered,xuiCodeDocumentGetText(core)) && xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),recovered)!=XUI_OK) { clearMetadata(document); failure(error,XIDE_MODEL_INVALID,"Cannot restore recovered text."); return NULL; }
    return document;
}


typedef struct relocation_item {
    xide_document* document;
    uint64_t id; uint32_t version; xui_widget editor; int window,dirty,override,bom,eol,changed;
    char *before,*title,*language,*session,*baselineText,*target,*nextTitle;
    unsigned char* raw; size_t rawLength;
    xide_task_stage* stageReference;
} relocation_item;
struct xide_document_relocation { xide_document_store* store; relocation_item* items; size_t count; int applied; };
static int sameText(const char* a,const char* b) { return (!a && !b) || (a && b && !strcmp(a,b)); }
void xideDocumentsDestroyRelocation(xide_document_relocation* plan)
{
    if(!plan) return;
    for(size_t i=0;plan->items && i<plan->count;++i) {
        relocation_item* p=&plan->items[i];
        free(p->before); free(p->title); free(p->language); free(p->session); free(p->baselineText);
        free(p->target); free(p->nextTitle); free(p->raw);
        xideTaskStageRelease(p->stageReference);
    }
    free(plan->items); free(plan);
}
xide_document_relocation* xideDocumentsPrepareRelocation(xide_document_store* store,xide_document_path_mapper mapper,void* user,xide_model_error* error)
{
    if(!store) { failure(error,XIDE_MODEL_INVALID,"无效文档集合。"); return NULL; }
    xide_document_relocation* plan=calloc(1,sizeof(*plan));
    if(!plan) { failure(error,XIDE_MODEL_MEMORY,"无法准备文档迁移。"); return NULL; }
    plan->store=store;
    for(size_t i=0;i<store->count;++i) if(store->items[i]->open) ++plan->count;
    plan->items=plan->count ? calloc(plan->count,sizeof(*plan->items)) : NULL;
    if(plan->count && !plan->items) goto memory;
    size_t index=0;
    for(size_t i=0;i<store->count;++i) {
        xide_document* d=store->items[i]; if(!d->open) continue;
        relocation_item* p=&plan->items[index++];
        p->document=d; p->id=d->id; p->editor=d->editor; p->window=d->window;
        p->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); p->dirty=d->dirty; p->override=d->languageOverride;
        p->bom=d->baseline.bom; p->eol=d->baseline.eol; p->rawLength=d->baseline.rawLength;
        p->before=d->path ? copy(d->path) : NULL; p->title=copy(d->title); p->language=copy(d->language); p->session=copy(d->sessionId);
        p->baselineText=d->baseline.text ? copy(d->baseline.text) : NULL;
        p->raw=p->rawLength ? malloc(p->rawLength) : NULL;
        p->target=d->path ? (mapper ? mapper(d->path,user) : copy(d->path)) : NULL;
        if((d->path && (!p->before || !p->target)) || !p->title || !p->language || !p->session || (d->baseline.text && !p->baselineText) || (p->rawLength && !p->raw)) goto memory;
        if(p->rawLength) memcpy(p->raw,d->baseline.raw,p->rawLength);
        p->changed=d->path && strcmp(d->path,p->target);
        if(p->changed) {
            if(strcmp(xideLanguageForPath(d->path),xideLanguageForPath(p->target))) { failure(error,XIDE_MODEL_INVALID,"批量迁移必须保留文档自动语法种类。"); goto failed; }
            p->nextTitle=copy(xidePathFilename(p->target)); if(!p->nextTitle) goto memory;
            xide_model_error protectionError={0}; p->stageReference=xideTaskStageProtectPath(p->target,&protectionError);
            if(protectionError.code) { if(error) *error=protectionError; goto failed; }
        }
    }
    for(size_t i=0;i<plan->count;++i) for(size_t j=0;j<i;++j)
        if(plan->items[i].target && plan->items[j].target && xidePathEqual(plan->items[i].target,plan->items[j].target)) { failure(error,XIDE_MODEL_CONFLICT,"迁移目标作为另一个文档打开，请先处理目标文档。"); goto failed; }
    return plan;
memory:
    failure(error,XIDE_MODEL_MEMORY,"无法预分配文档迁移路径和独立基线。");
failed:
    xideDocumentsDestroyRelocation(plan); return NULL;
}
int xideDocumentsValidateRelocation(const xide_document_relocation* plan,xide_model_error* error)
{
    if(!plan || plan->applied) return failure(error,XIDE_MODEL_INVALID,"文档迁移事务已结束。");
    size_t count=0;
    for(size_t i=0;i<plan->store->count;++i) {
        xide_document* d=plan->store->items[i]; if(!d->open) continue;
        const relocation_item* p=NULL;
        for(size_t j=0;j<plan->count;++j) if(plan->items[j].id==d->id) { p=&plan->items[j]; break; }
        if(!p || p->document!=d || p->editor!=d->editor || p->window!=d->window || !sameText(p->before,d->path) || !sameText(p->title,d->title) || !sameText(p->language,d->language) || !sameText(p->session,d->sessionId) || p->dirty!=d->dirty || p->override!=d->languageOverride || p->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) || p->bom!=d->baseline.bom || p->eol!=d->baseline.eol || p->rawLength!=d->baseline.rawLength || !sameText(p->baselineText,d->baseline.text) || (p->rawLength && (!d->baseline.raw || memcmp(p->raw,d->baseline.raw,p->rawLength))))
            return failure(error,XIDE_MODEL_CONFLICT,"打开文档或编辑状态已改变，请重新打开迁移窗口。");
        ++count;
    }
    return count==plan->count ? 1 : failure(error,XIDE_MODEL_CONFLICT,"文档已关闭，请重新准备迁移。");
}
void xideDocumentsApplyRelocation(xide_document_relocation* plan)
{
    if(!plan || plan->applied) return;
    /* Bind the whole batch before notifying UI; code/undo/selection are untouched. */
    for(size_t i=0;i<plan->count;++i) {
        relocation_item* p=&plan->items[i]; if(!p->changed) continue;
        xide_document* d=p->document; free(d->path); free(d->title); d->path=p->target; d->title=p->nextTitle;
        p->target=p->nextTitle=NULL; d->externalState=0;
        /* A whole-project move keeps the original physical marker lease when
           the destination does not exist during preparation. */
        if(p->stageReference) { xideTaskStageRelease(d->stageReference); d->stageReference=p->stageReference; p->stageReference=NULL; }
    }
    plan->applied=1;
    for(size_t i=0;i<plan->count;++i) {
        relocation_item* p=&plan->items[i]; if(!p->changed) continue;
        xide_document* d=p->document;
        if(plan->store->pathChanged) plan->store->pathChanged(d,p->before,d->path,plan->store->user);
        if(plan->store->onChanged) plan->store->onChanged(d,plan->store->user);
    }
}
