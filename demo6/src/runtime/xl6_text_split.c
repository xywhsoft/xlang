#include "src/runtime/xl6_text_pieces.h"

/* XRT owns all delimiter/line/field semantics. The language adapter only
 * turns borrowed pieces into independently owned string values. */
static xvalue* xl6TextPieces(xstrview text, xstrview separator, unsigned mode)
{
    XL6_TextPieces pieces;
    if (!xl6TextPiecesInit(&pieces, text, separator, mode)) return NULL;
    xvalue* result = xrtValueArray();
    if (!result) return NULL;
    xstrview item;
    while (xl6TextPiecesNext(&pieces, &item, mode)) {
        xvalue* value = xrtValueString(item);
        if (!value) goto fail;
        /* AppendNew consumes the item on both success and failure. */
        if (!xrtValueArrayAppendNew(result, value)) goto fail;
    }
    return result;
fail:
    xrtValueRelease(result);
    return NULL;
}
xvalue* xl6TextSplitV1(xstrview text, xstrview separator, int64_t size, int64_t separatorSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&separator, separatorSize)) return NULL;
    return xl6TextPieces(text, separator, 0);
}
xvalue* xl6TextLinesV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    return xl6TextPieces(text, (xstrview){NULL, 0}, 1);
}
xvalue* xl6TextFieldsV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    return xl6TextPieces(text, (xstrview){NULL, 0}, 2);
}
