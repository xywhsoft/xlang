#include "src/runtime/xl6_text_pieces.h"
#include "src/runtime/xl6_array_producer.h"

/* XRT owns all delimiter/line/field semantics. The language adapter only
 * turns borrowed pieces into independently owned string values. */
static xvalue* xl6TextPieces(uint64_t identity, uint64_t element, xstrview text, xstrview separator, unsigned mode)
{
    XL6_TextPieces pieces;
    if (!xl6TextPiecesInit(&pieces, text, separator, mode)) return NULL;
    XL6_ArrayProducerV1 result;
    if (!xl6ArrayProducerBeginV1(&result,identity,element,XL6_VALUE_ADMIT_STRING,xl6StringTypeV1(),xl6StringConverterV1())) goto fail;
    xstrview item;
    while (xl6TextPiecesNext(&pieces, &item, mode)) {
        if (!xl6ArrayProducerPushV1(&result,&item)) goto fail;
    }
    return xl6ArrayProducerFinishV1(&result,true);
fail:
    return xl6ArrayProducerFinishV1(&result,false);
}
xvalue* xl6TextSplitV2(uint64_t identity, uint64_t element, xstrview text, xstrview separator, int64_t size, int64_t separatorSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&separator, separatorSize)) return NULL;
    return xl6TextPieces(identity,element,text,separator,0);
}
xvalue* xl6TextLinesV2(uint64_t identity, uint64_t element, xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    return xl6TextPieces(identity,element,text,(xstrview){NULL,0},1);
}
xvalue* xl6TextFieldsV2(uint64_t identity, uint64_t element, xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    return xl6TextPieces(identity,element,text,(xstrview){NULL,0},2);
}
