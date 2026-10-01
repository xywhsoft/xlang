#include "src/runtime/xl6_text_pieces.h"

typedef struct XL6_TextIterator {
    uint64_t ownerThread; /* Immutable, checked before reading mutable state. */
    XL6_TextPieces pieces;
    char* text;
    char* separator;
    unsigned mode;
    bool closed;
} XL6_TextIterator;

static XL6_TextIterator* xl6TextIteratorCheck(void* handle)
{
    XL6_TextIterator* it = handle;
    if (!it) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (it->ownerThread != xrtThreadCurrentId()) { xrtSetErrorKind(XERR_STATE); return NULL; }
    return it;
}
static void xl6TextIteratorReleaseInput(XL6_TextIterator* it)
{
    char* text = it->text;
    char* separator = it->separator;
    it->closed = true;
    it->text = NULL; it->separator = NULL;
    /* Establish the closed invariant before calling allocator hooks. */
    xrtFree(text); xrtFree(separator);
}
void xl6TextIteratorDestroyV1(void* handle)
{
    /* Managed resource retirement serializes final destruction; it may occur
     * on a different thread and must neither allocate nor replace an error. */
    XL6_TextIterator* it = handle;
    if (!it) return;
    xl6TextIteratorReleaseInput(it); xrtFree(it);
}
void* xl6TextIteratorCreateV1(xstrview text, xstrview separator, int64_t mode, int64_t size, int64_t separatorSize)
{
    if (mode < 0 || mode > 2) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&separator, separatorSize)) return NULL;
    XL6_TextIterator* it = xrtCalloc(1, sizeof(*it));
    if (!it) return NULL;
    it->ownerThread = xrtThreadCurrentId(); it->mode = (unsigned)mode;
    it->text = xrtStrDupView(text);
    if (!it->text) goto fail;
    if (mode == 0) {
        it->separator = xrtStrDupView(separator);
        if (!it->separator) goto fail;
    }
    if (!xl6TextPiecesInit(&it->pieces, (xstrview){it->text, text.Size},
            (xstrview){it->separator, mode == 0 ? separator.Size : 0}, it->mode)) goto fail;
    return it;
fail:
    xl6TextIteratorDestroyV1(it); return NULL;
}
xstrview xl6TextIteratorNextV1(void* handle)
{
    XL6_TextIterator* it = xl6TextIteratorCheck(handle);
    if (!it || it->closed) return xl6TextFailure;
    XL6_TextPieces checkpoint = it->pieces;
    xstrview item;
    if (!xl6TextPiecesNext(&checkpoint, &item, it->mode)) {
        xl6TextIteratorReleaseInput(it); return xl6TextFailure;
    }
    xstrview result = xl6TextCopy(item);
    /* Do not consume an item on allocation failure: next() is retryable. */
    if (result.Data) it->pieces = checkpoint;
    return result;
}
void xl6TextIteratorCloseV1(void* handle)
{
    XL6_TextIterator* it = xl6TextIteratorCheck(handle);
    if (it) xl6TextIteratorReleaseInput(it);
}
bool xl6TextIteratorClosedV1(void* handle)
{
    XL6_TextIterator* it = xl6TextIteratorCheck(handle);
    return it ? it->closed : false;
}
