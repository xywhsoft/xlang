#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#include "../money_native.c"
#include <stdio.h>

/* C 也可使用同一装箱入口；结果取出后立即释放，错误单独读取。 */
static int consume(xvalue* value, int64_t* out)
{
    int ok = value && !xrtGetError() && xrtValueGetInt(value,out);
    xrtValueRelease(value);
    return ok;
}

int main(void)
{
    int64_t subtotal,refund,amount;
    if (!consume(xm_add(799,349),&subtotal) || !consume(xm_sub(subtotal,199),&refund)) return 1;
    printf("subtotal=%lld\nrefund=%lld\n",(long long)subtotal,(long long)refund);
    if (!consume(xm_mul(125,3),&amount)) return 1;
    printf("bundle=%lld\n",(long long)amount);
    if (!consume(xm_scale(refund,9,10),&amount)) return 1;
    printf("discount=%lld\n",(long long)amount);
    if (!consume(xm_rate(125,0.5),&amount)) return 1;
    printf("estimate=%lld\n",(long long)amount);
    xvalue* value = xm_add(INT64_MAX,1);
    int rejected = value && xrtValueType(value)==XVALUE_NULL && xrtErrorKind(xrtGetError())==XERR_RANGE;
    xrtValueRelease(value);
    if (!rejected) return 1;
    puts("overflow=rejected");
    if (!consume(xm_add(1,2),&amount) || amount!=3) return 1;
    printf("recovered=%lld\n",(long long)amount);
    return ferror(stdout) ? 1 : 0;
}
