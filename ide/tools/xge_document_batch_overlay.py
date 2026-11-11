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
#define XUI_CODE_DOCUMENT_BATCH 1
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
'''

def derive(root):
    root=Path(root)
    header=root/'include/xui.h';source=root/'src/xui/code/xui_code_document.c'
    payload=Path(__file__).with_name('xge_document_batch.inc').read_bytes()
    changes={}
    for path,anchor,replacement in (
        (header,b'XUI_API int xuiCodeDocumentCanRedo(xui_code_document pDocument);',
         b'XUI_API int xuiCodeDocumentCanRedo(xui_code_document pDocument);\n'+HEADER.encode()),
        (source,b'\n#endif\n',b'\n'+payload+b'\n#endif\n')):
        original=path.read_bytes();newline=b'\r\n'if b'\r\n'in original else b'\n'
        a=anchor.replace(b'\n',newline);b=replacement.replace(b'\n',newline)
        if b'XUI_CODE_DOCUMENT_BATCH'in original or original.count(a)!=1:
            raise ValueError(f'{path}: unexpected or already modified source')
        result=original.replace(a,b,1);path.write_bytes(result)
        changes[path.relative_to(root).as_posix()]=dict(before=hashlib.sha256(original).hexdigest(),after=hashlib.sha256(result).hexdigest())
    return changes

if __name__=='__main__':
    import argparse,json
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path)
    print(json.dumps(derive(parser.parse_args().source),indent=2))
