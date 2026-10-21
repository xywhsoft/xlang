#ifndef XLANG_IDE_DOCUMENTS_H
#define XLANG_IDE_DOCUMENTS_H
#include "platform/text_file.h"
#include "platform/file_rename.h"
#include "platform/task_stage.h"
#include "xui.h"
typedef struct xide_document_store xide_document_store;
typedef struct xide_document {
    xide_document_store* store;
    uint64_t id;
    char *path,*title,*language,*sessionId;
    xide_text_file baseline;
    xide_task_stage* stageReference;
    xui_widget editor; /* XUI owns this widget and its text/undo document. */
    int window,open,dirty,suppressChange,languageOverride,externalState;
    uint32_t findVersion; /* UI-only baseline for shared find results. */
    xvalue* cAnalysisContext; /* Project/source association from verified C navigation. */
} xide_document;
typedef void (*xide_document_changed_proc)(xide_document* document,void* user);
struct xide_document_store {
    xui_context context;
    xui_font font;
    xide_document** items;
    size_t count,capacity;
    uint64_t nextId;
    xide_document_changed_proc onChanged;
    int (*beforeSave)(xide_document* document,const char* target,xide_model_error* error,void* user);
    void (*afterSave)(xide_document* document,void* user);
    void (*pathChanged)(xide_document* document,const char* previous,const char* target,void* user);
    /* One completion after path resolution, including refusal/no-op/failure. */
    void (*saveFinished)(xide_document* document,int saved,void* user);
    void* user;
};
enum { XIDE_DOCUMENT_OVERWRITE_EXTERNAL=1, XIDE_DOCUMENT_OVERWRITE_TARGET=2 };
void xideDocumentsInit(xide_document_store* store,xui_context context,xui_font font,xide_document_changed_proc onChanged,void* user);
/* Call before xuiDestroy: callbacks are detached, widget ownership remains with XUI. */
void xideDocumentsClear(xide_document_store* store);
xide_document* xideDocumentsFind(xide_document_store* store,const char* path);
xide_document* xideDocumentOpen(xide_document_store* store,const char* path,xide_model_error* error);
xide_document* xideDocumentNew(xide_document_store* store,const char* language,xide_model_error* error);
xide_document* xideDocumentRecover(xide_document_store* store,const char* path,const char* title,const char* language,xide_text_file* baseline,const char* recovered,xide_model_error* error);
int xideDocumentSave(xide_document* document,const char* path,int flags,xide_model_error* error);
int xideDocumentRenameSource(xide_document* document,const char* source,const char* target,const xide_file_snapshot* sourceBaseline,xide_model_error* error);
int xideDocumentRename(xide_document* document,const char* target,xide_model_error* error);
/* Moves a decoded worker snapshot only on success; preserves identity, undo and reading positions. */
int xideDocumentAcceptExternal(xide_document* document,xide_text_file* snapshot,int discardChanges,xide_model_error* error);
int xideDocumentReload(xide_document* document,int discardChanges,xide_model_error* error);
int xideDocumentClose(xide_document* document,int discardChanges,xide_model_error* error);
int xideDocumentSetLanguage(xide_document* document,const char* language,xide_model_error* error);
int xideDocumentCheckExternal(xide_document* document);
int xideDocumentsDirty(const xide_document_store* store);
int xideDocumentCContextValid(const xvalue* context);
/* Metadata-only batch for a physical move performed by the caller. NULL mapper
   captures an unchanged store baseline. Returned mapper paths use malloc/free.
   Validate immediately before moving, then Apply without dispatching events.
   Apply allocates no path/title and preserves code documents and their baselines. */
typedef struct xide_document_relocation xide_document_relocation;
typedef char* (*xide_document_path_mapper)(const char* path,void* user);
xide_document_relocation* xideDocumentsPrepareRelocation(xide_document_store* store,xide_document_path_mapper mapper,void* user,xide_model_error* error);
int xideDocumentsValidateRelocation(const xide_document_relocation* plan,xide_model_error* error);
void xideDocumentsApplyRelocation(xide_document_relocation* plan);
void xideDocumentsDestroyRelocation(xide_document_relocation* plan);
#endif
