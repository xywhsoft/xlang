"""Add the prepared multi-document primitive to an exact XUI source cohort.

Keep SDK changes in its producer implementation, not in a private IDE struct
clone. Changes are additive; a matched SDK rebuild is mandatory. Never modifies
a binary or imports arbitrary current upstream work into the frozen IDE cohort.
"""
from pathlib import Path
import hashlib

HEADER = '''
/* Owner-thread, single-attempt prepared document batch. Input views live only
 * during Prepare; Before must match the current LF UTF8 document and its exact
 * change version. Duplicate documents, nested edits, invalid UTF8/NUL/CR and
 * stale/closed owners are refused. Max512 documents,8MiB/body,64MiB before/after
 * totals; default128MiB/max256MiB known staging storage. The caller's inputs,
 * allocator and observer/cache work are additional, not an RSS/latency bound.
 * Prepare allocates replacement buffers, full line indexes and one undo group
 * per changed document without changing content, versions, dirty or history.
 * Commit pins all documents, runs preflight observers and rechecks the group;
 * it publishes every payload without allocation before any edit/change notice.
 * Preflight may invalidate derived caches. Each document gains one undo step;
 * its earlier undo history is preserved and normal edit semantics clear redo.
 * Commit core does not allocate. Observers/cleanup may allocate, close owners
 * or fail after publication: ALWAYS inspect *committed, even on an error return.
 * true means all payloads committed, including an empty/no-op accepted batch.
 * false means none committed. The batch is single-attempt and must be destroyed
 * after either outcome. Destroy/Commit reentry or another thread returns BUSY.
 * Document destruction invalidates prepared rows without retaining the owner.
 * No disk transaction, caret mapping, global undo or IDE revision policy is
 * implied. Normal Document observer/callback and owner lifetime rules apply. */
#define XUI_CODE_DOCUMENT_BATCH 2
typedef struct xui_code_document_batch_t* xui_code_document_batch;
typedef struct xui_code_document_batch_item_t {
    xui_code_document pDocument;
    uint32_t iChangeVersion;
    xstrview Before,After;
} xui_code_document_batch_item_t;
XUI_API int xuiCodeDocumentBatchPrepare(const xui_code_document_batch_item_t* pItems,
    size_t iCount,size_t iBudget,xui_code_document_batch* ppBatch);
XUI_API int xuiCodeDocumentBatchCommit(xui_code_document_batch pBatch,bool* pCommitted);
XUI_API int xuiCodeDocumentBatchDestroy(xui_code_document_batch pBatch);

/* Split preparation. Capture/Describe run on the owner and copy metadata only;
 * Capture ignores the legacy item's Before/After fields. Describe outputs a
 * detached, copyable layout/source description with no GUI pointer or lease.
 * Stage may run on any thread from immutable views held for that call only.
 * It owns every body/index/history allocation, no live Document/Context links.
 * Pass one payload owner between threads with external synchronization.
 * Accept is owner-only: recheck capture identity, versions/states and exact
 * before bytes (at most two memcmp spans/document), then move allocations.
 * Accept allocates/copies no body or index; it DOES scan before bytes. Undo
 * prefix metadata is copied in Commit. ALWAYS inspect *taken: true consumes
 * the payload even if a later call-scope cleanup returns an error. Failed
 * acceptance leaves it caller-owned. A capture cannot commit until accepted.
 * Retire unaccepted payloads with PayloadDestroy on any thread, sequentially.
 * Never concurrently stage from mutable inputs, accept/destroy one payload,
 * or call owner APIs from the worker. Cancelled Stage returns INVALID_STATE
 * and NULL output; local polls are not an end-to-end cancellation bound.
 * Existing Prepare remains a synchronous convenience over this same pipeline.
 * The known staging quota includes Capture storage and transient payload rows;
 * raw body/index arrays are fully written without redundant zero-filling. */
typedef struct xui_code_document_batch_layout_t {
    uint32_t iSize;
    uint64_t iCapture;
    size_t iDocuments,iCaptureBytes;
} xui_code_document_batch_layout_t;
typedef struct xui_code_document_batch_source_t {
    uint32_t iSize;
    int iLength,iUndoCount;
} xui_code_document_batch_source_t;
typedef struct xui_code_document_batch_text_t {
    uint32_t iSize;
    xstrview Before,After;
} xui_code_document_batch_text_t;
typedef struct xui_code_document_batch_payload_t* xui_code_document_batch_payload;
XUI_API int xuiCodeDocumentBatchCapture(const xui_code_document_batch_item_t* pTargets,
    size_t iCount,size_t iBudget,xui_code_document_batch* ppBatch);
XUI_API int xuiCodeDocumentBatchDescribe(xui_code_document_batch pBatch,
    xui_code_document_batch_layout_t* pLayout,xui_code_document_batch_source_t* pSources,size_t iCapacity);
XUI_API int xuiCodeDocumentBatchStage(const xui_code_document_batch_layout_t* pLayout,
    const xui_code_document_batch_source_t* pSources,const xui_code_document_batch_text_t* pTexts,
    size_t iBudget,int (*cancelled)(void*),void* pUser,xui_code_document_batch_payload* ppPayload);
XUI_API int xuiCodeDocumentBatchAccept(xui_code_document_batch pBatch,
    xui_code_document_batch_payload pPayload,bool* pTaken);
XUI_API void xuiCodeDocumentBatchPayloadDestroy(xui_code_document_batch_payload pPayload);

/* Optional synchronous host gates; callbacks/user are borrowed for Commit only.
 * beforePublish runs after toolkit preflight, may refuse without publication,
 * and is followed by another whole-group toolkit fence. published runs after
 * all payloads and *committed=true, before any toolkit edit/change observer.
 * published must be an infallible, preallocated model publication: do not
 * allocate, dispatch callbacks or close owners there. It is not rollback.
 * This permits the IDE's identity/config fences and dirty/store metadata to
 * surround publication, without interleaving per-document change notices. */
typedef struct xui_code_document_batch_callbacks_t {
    uint32_t iSize;
    int (*beforePublish)(void*);
    void (*published)(void*);
    void* pUser;
} xui_code_document_batch_callbacks_t;
XUI_API int xuiCodeDocumentBatchCommitChecked(xui_code_document_batch pBatch,
    const xui_code_document_batch_callbacks_t* pCallbacks,bool* pCommitted);
'''

def derive(root):
    root=Path(root)
    header=root/'include/xui.h';source=root/'src/xui/code/xui_code_document.c'
    payload=Path(__file__).with_name('xge_document_batch.inc').read_text(encoding='utf8')
    marker='/* XUI_DOCUMENT_BATCH_WORKER */'
    if payload.count(marker)!=1:
        raise ValueError('Missing worker implementation marker')
    payload=payload.replace(marker,Path(__file__).with_name('xge_document_batch_worker.inc').read_text(encoding='utf8'),1)
    changes={}
    planned=[]
    def canonical(text):return (text.replace('\r\n','\n').strip()+'\n').encode()
    for path in (header,source):
        original=path.read_bytes();text=original.decode('utf8');newline='\r\n'if '\r\n'in text else '\n'
        if path==header:
            if '#define XUI_CODE_DOCUMENT_BATCH 1'in text:
                a=text.index('/* Owner-thread, single-attempt prepared document batch.')
                end='XUI_API int xuiCodeDocumentBatchDestroy(xui_code_document_batch pBatch);'
                b=text.index(end,a)+len(end)
                if hashlib.sha256(canonical(text[a:b])).hexdigest()!='7f4baa9a876457c5c00ac925a11fa8dfee5c08e2b6ba9408cf2eb1051afbcbc5':
                    raise ValueError('Version1 public block differs from qualified source')
                text=text[:a]+HEADER.strip().replace('\n',newline)+text[b:]
            elif 'XUI_CODE_DOCUMENT_BATCH'not in text:
                anchor='XUI_API int xuiCodeDocumentCanRedo(xui_code_document pDocument);'
                if text.count(anchor)!=1:raise ValueError('Missing public anchor')
                text=text.replace(anchor,anchor+HEADER.replace('\n',newline),1)
            else:raise ValueError('Unexpected/already upgraded public source')
        else:
            start='/* Additive XUI document primitive.';end=newline+'#endif'+newline
            if text.count(end)!=1:raise ValueError('Unrecognized Document end')
            b=text.index(end)
            if start in text:
                a=text.index(start)
                if hashlib.sha256(canonical(text[a:b])).hexdigest()!='fa632d31d99e3d8f6fb318f12e5d632091e9c5bc22c5fab5124dabc97e6c7283':
                    raise ValueError('Version1 implementation differs from qualified source')
                text=text[:a]+payload.replace('\n',newline)+text[b:]
            else:text=text[:b]+newline+payload.replace('\n',newline)+text[b:]
            before='if ( pDocument == NULL || pAction == NULL || iStart < 0 || iDeletedLength < 0 || iInsertedLength < 0 ) {'
            after='if ( pAction == NULL || iStart < 0 || iDeletedLength < 0 || iInsertedLength < 0 || (pDocument == NULL && iDeletedLength > 0 && sDeleted == NULL) ) {'
            if text.count(before)!=1:raise ValueError('Unrecognized shared action initializer')
            text=text.replace(before,after,1)
        result=text.encode('utf8');planned.append((path,result))
        changes[path.relative_to(root).as_posix()]=dict(before=hashlib.sha256(original).hexdigest(),after=hashlib.sha256(result).hexdigest())
    for path,result in planned:path.write_bytes(result)
    return changes

if __name__=='__main__':
    import argparse,json
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path)
    print(json.dumps(derive(parser.parse_args().source),indent=2))
