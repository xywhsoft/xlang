#include "src/runtime/xl6_buffer_internal.h"
#include "src/runtime/xl6_text_thread.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Independent byte snapshot: no parent pointer, language edges or generated
 * callback. Creation costs O(n); traversal is O(1) per byte. Close releases
 * the copied bytes immediately while keeping a valid exhausted iterator. */
typedef struct XL6_BufferSnapshot {
    XL6_TextThreadOwner thread;
    unsigned char* data;
    size_t size, index;
    bool closed;
} XL6_BufferSnapshot;
static XL6_BufferSnapshot* bufferSnapshotCheck(void* raw)
{
    XL6_BufferSnapshot* snapshot = raw;
    if (!snapshot) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    /* Mutable traversal state is creation-thread confined. Final resource
     * retirement may run elsewhere after managed ownership serializes it. */
    if (!xl6TextThreadCheck(&snapshot->thread)) return NULL;
    return snapshot;
}
static void bufferSnapshotRelease(XL6_BufferSnapshot* snapshot)
{
    unsigned char* data = snapshot->data;
    /* Allocator hooks may reenter close: publish the detached state first. */
    snapshot->data = NULL;
    snapshot->size = snapshot->index = 0;
    snapshot->closed = true;
    xrtFree(data);
}
void xl6BufferIteratorDestroyV1(void* raw)
{
    XL6_BufferSnapshot* snapshot = raw;
    if (!snapshot) return;
    xerror* prior = xrtTakeError();
    bufferSnapshotRelease(snapshot);
    xl6TextThreadDrop(&snapshot->thread);
    xrtFree(snapshot);
    xrtClearError();
    xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6BufferIteratorHandlePolicyV1Get, xl6BufferIteratorDestroyV1)
void* xl6BufferIteratorCreateV1(void* source, int64_t size)
{
    xbytesview view;
    if (!xl6BufferInput(source, size, &view)) return NULL;
    XL6_BufferSnapshot* snapshot = xrtCalloc(1, sizeof(*snapshot));
    if (!xl6BufferAllocation(snapshot != NULL)) return NULL;
    if (!xl6TextThreadInit(&snapshot->thread)) {
        xl6BufferIteratorDestroyV1(snapshot);
        return NULL;
    }
    if (view.Size) {
        snapshot->data = xrtMalloc(view.Size);
        if (!xl6BufferAllocation(snapshot->data != NULL)) {
            xl6BufferIteratorDestroyV1(snapshot);
            return NULL;
        }
        memcpy(snapshot->data, view.Data, view.Size);
    }
    snapshot->size = view.Size;
    return snapshot;
}
bool xl6BufferIteratorHasNextV1(void* raw)
{
    XL6_BufferSnapshot* snapshot = bufferSnapshotCheck(raw);
    if (!snapshot) return false;
    return !snapshot->closed && snapshot->index < snapshot->size;
}
int64_t xl6BufferIteratorNextV1(void* raw)
{
    XL6_BufferSnapshot* snapshot = bufferSnapshotCheck(raw);
    if (!snapshot) return 0;
    if (snapshot->closed || snapshot->index >= snapshot->size) {
        xrtSetErrorKind(XERR_STATE);
        return 0;
    }
    return snapshot->data[snapshot->index++];
}
void xl6BufferIteratorCloseV1(void* raw)
{
    XL6_BufferSnapshot* snapshot = bufferSnapshotCheck(raw);
    if (snapshot) bufferSnapshotRelease(snapshot);
}
bool xl6BufferIteratorClosedV1(void* raw)
{
    XL6_BufferSnapshot* snapshot = bufferSnapshotCheck(raw);
    if (!snapshot) return false;
    return snapshot->closed;
}
