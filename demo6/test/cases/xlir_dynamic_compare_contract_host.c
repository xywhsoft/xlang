#include <math.h>
#include <stdio.h>
#include "../../build/test/xlir_dynamic_compare_contract_generated.c"

static int expectMask(xvalue* left, xvalue* right, int64_t expected, const char* label)
{
    int64_t actual = xlir_dynamic_compare_mask(left, right);
    if (actual != expected || xrtGetError() != NULL) {
        fprintf(stderr, "dynamic comparison %s: actual=%lld expected=%lld error=%p\n",
            label, (long long)actual, (long long)expected, (void*)xrtGetError());
        return 0;
    }
    return 1;
}

int main(void)
{
    xvalue* bytes;
    xvalue* time;
    xvalue* nan;
    xvalue* inf;
    xvalue* negativeInf;
    xvalue* integer;
    int ok;
    unsigned char data[] = {1, 2};
    xbytesview view = {data, sizeof(data)};
    if (!xlir_module_init()) return 1;
    bytes = xrtValueBytes(view);
    time = xrtValueTime(0);
    nan = xrtValueFloat(NAN);
    inf = xrtValueFloat(INFINITY);
    negativeInf = xrtValueFloat(-INFINITY);
    integer = xrtValueInt(INT64_MAX);
    if (!bytes || !time || !nan || !inf || !negativeInf || !integer) return 2;
    /* Unsupported ordering must not call a mismatched XRT getter or set errors. */
    ok = expectMask(bytes, bytes, 1, "bytes identity") &&
         expectMask(bytes, time, 0, "bytes/time") &&
         expectMask(time, bytes, 0, "time/bytes") &&
         expectMask(nan, nan, 0, "NaN identity is not numeric equality") &&
         expectMask(nan, integer, 0, "NaN/number") &&
         expectMask(integer, nan, 0, "number/NaN") &&
         expectMask(inf, integer, 24, "+inf/number") &&
         expectMask(integer, inf, 6, "number/+inf") &&
         expectMask(negativeInf, integer, 6, "-inf/number") &&
         expectMask(xrtValueNull(), xrtValueNull(), 21, "canonical null");
    /* Raw NULL is not a valid owned var export argument. Test normalization at
     * the borrowed helper boundary without invoking the export retain adapter. */
    ok = ok && xlir_xvalue_equal(NULL, xrtValueNull()) &&
         xlir_xvalue_equal(xrtValueNull(), NULL) &&
         xlir_xvalue_order(NULL, xrtValueNull(), 1) &&
         !xlir_xvalue_approx(NULL, xrtValueNull()) && xrtGetError() == NULL;
    xrtValueRelease(bytes); xrtValueRelease(time); xrtValueRelease(nan);
    xrtValueRelease(inf); xrtValueRelease(negativeInf); xrtValueRelease(integer);
    return ok ? 0 : 3;
}
