#ifndef XLANG_IDE_DOCUMENTS_H
#define XLANG_IDE_DOCUMENTS_H
#include "platform/text_file.h"
#include "platform/file_rename.h"
#include "platform/task_stage.h"
#include "xui.h"
typedef struct xide_document_store xide_document_store;
typedef struct xide_document_baseline xide_document_baseline;
typedef struct xide_document {
    xide_document_store* store;
    uint64_t id;
    /* Owner-thread body clock, independent of the toolkit's wrapping uint32.
     * Starts at1 per id;0 means closed/exhausted and cannot authorize a snapshot.
     * Every content notice advances even while model refresh is suppressed. */
    uint64_t revision;
    uint32_t preparedVersion;
    int preparedChange;
    char *path,*title,*language,*sessionId;
    xide_text_file baseline;
    /* Read-only view while baselineLease exists; all replacement/clear belongs
     * to documents.c. The lease owns the original malloc buffers, without copy. */
    xide_document_baseline* baselineLease;
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
/* Verified worker snapshot;no source read,decode or hash here. Moves buffers
 * only on success. Existing open paths are refused to preserve their undo. */
xide_document* xideDocumentOpenPrepared(xide_document_store* store,const char* path,xide_text_file* snapshot,xide_model_error* error);
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
/* Owner capture: lazily shares the saved buffers with XRT's checked native
 * reference counter; no body scan/copy or editor access on worker readers.
 * Failure leaves the document baseline owned and unchanged. Subsequent save,
 * reload, close and store destruction may replace the model view while leases
 * keep the old bytes alive. Release on any thread, sequentially per owner.
 * A returned snapshot is immutable and borrowed until that lease is released.
 * Known bytes conservatively include both raw/text capacities plus wrapper;
 * not allocator/RSS. It may keep up to16MiB raw+normalized bytes per baseline. */
xide_document_baseline* xideDocumentRetainBaseline(xide_document* document);
const xide_text_file* xideDocumentBaselineRead(const xide_document_baseline*);
size_t xideDocumentBaselineStorage(const xide_document_baseline*);
void xideDocumentBaselineRelease(xide_document_baseline*);
/* Infallible model publication for the XUI batch published hook ONLY. Caller
 * has validated/pinned this open document/core, nonzero/non-exhausted revision,
 * baseline and dirty
 * result, and publishes the entire group before allowing any change notice.
 * No allocation, body comparison, syntax refresh or application callback.
 * The expected toolkit notice consumes this marker instead of double-advancing
 * revision/recomputing dirty. Every unrelated notice resumes ordinary behavior.
 * Always Finish after Commit returns, including post-publication failure or a
 * skipped notice. These helpers are not admission/consent or a transaction. */
void xideDocumentPublishPrepared(xide_document*,xui_code_document core,int dirty);
void xideDocumentFinishPrepared(xide_document*);
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
