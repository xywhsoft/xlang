/* 与 numeric_fields.xl 读取同一份标准 BSON，不用本库编码器构造参照。 */
#include "../bson_native.c"

int main(void)
{
    static const uint8 wire[] = {
        60,0,0,0, 16,'s','m','a','l','l',0,123,0,0,0,
        18,'l','a','r','g','e',0,0,0,0,0x80,0,0,0,0,
        1,'r','a','t','i','o',0,0,0,0,0,0,0,0xf4,0x3f,
        9,'w','h','e','n',0,0x0c,0xfe,0xff,0xff,0xff,0xff,0xff,0xff, 0
    };
    xvalue* document = xbson_decode_span((xbytesview){wire, sizeof(wire)});
    int64 small, large; double ratio; xtime time;
    bool ok = document != NULL && xrtGetError() == NULL &&
        xrtValueGetInt(xrtValueObjectGet(document, XRT_STR_LITERAL("small")), &small) && small == 123 &&
        xrtValueGetInt(xrtValueObjectGet(document, XRT_STR_LITERAL("large")), &large) && large == INT64_C(2147483648) &&
        xrtValueGetFloat(xrtValueObjectGet(document, XRT_STR_LITERAL("ratio")), &ratio) && ratio == 1.25 &&
        xrtValueGetTime(xrtValueObjectGet(document, XRT_STR_LITERAL("when")), &time) && time == -500000;
    char* hex = ok ? xbson_to_hex_span((xbytesview){wire, sizeof(wire)}) : NULL;
    int64_t length = xbson_doc_length_span((xbytesview){wire, sizeof(wire)});
    ok = ok && hex != NULL && strlen(hex) == 120 && length == 60;
    if (ok) printf("small=%lld\nlarge=%lld\nratio=%.2f\nnegative_date=true\nbytes=%lld\n",
        (long long)small, (long long)large, ratio, (long long)length);
    xrtFree(hex);
    xrtValueRelease(document);
    return ok ? 0 : 1;
}
