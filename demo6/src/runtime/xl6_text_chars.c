#include "src/runtime/xl6_text_internal.h"
#include "src/runtime/xl6_array_producer.h"

xvalue* xl6TextCharsV2(uint64_t identity, uint64_t element, xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    XL6_ArrayProducerV1 result;
    if (!xl6ArrayProducerBeginV1(&result,identity,element,XL6_VALUE_ADMIT_CHAR,xl6CharTypeV1(),xl6CharConverterV1())) goto fail;
    size_t count=xrtUtf8Count(text);
    if (count==XRT_NPOS || !xl6ArrayProducerReserveV1(&result,count)) goto fail;
    size_t index = 0;
    while (index < text.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){text.Data + index, text.Size - index}, &scalar, &read) != XUTF_OK) goto fail;
        if (!xl6ArrayProducerPushV1(&result,&scalar)) goto fail;
        index += read;
    }
    return xl6ArrayProducerFinishV1(&result,true);
fail:
    return xl6ArrayProducerFinishV1(&result,false);
}
