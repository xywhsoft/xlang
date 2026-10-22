#include "../include/money_native.h"
#include "../money_native.c"

#include <stdio.h>
#include <string.h>

static int print_amount(const char *label, int64_t amount) {
    char *text = (char *)xm_format(amount, "USD", 3);
    if (text == NULL) return 0;
    printf("%s%s\n", label, text);
    xrtFree(text);
    return 1;
}

int main(void) {
    const int64_t prices[] = {7999, 3499, 1200, 1500};
    const char *currencies[] = {"USD", "EUR", "CNY", "JPY", "GBP", "RUB"};
    int64_t subtotal = 0, discounted, taxed;
    xvalue *parts;
    size_t i;

    /* 与 XLang 范例逐项对应：结算、精确比例、分摊、多币种展示。 */
    for (i = 0; i < sizeof(prices) / sizeof(prices[0]); ++i) {
        if (!xm_add_checked(subtotal, prices[i], &subtotal)) return 1;
    }
    if (!print_amount("subtotal: ", subtotal) ||
        !xm_scale_checked(subtotal, 9, 10, &discounted) ||
        !xm_scale_checked(discounted, 107, 100, &taxed) ||
        !print_amount("after 10% off: ", discounted) ||
        !print_amount("after 7% tax: ", taxed)) return 1;

    /* 退货和件数使用精确整数；浮点比例只展示估算入口。 */
    {
        int64_t refund, bulk, estimate;
        if (!xm_sub_checked(subtotal, 1200, &refund) ||
            !xm_mul_checked(125, 3, &bulk) ||
            !xm_rate_checked(125, 0.5, &estimate) ||
            !print_amount("after refund: ", refund) ||
            !print_amount("three small items: ", bulk)) return 1;
        printf("floating estimate (cents): %lld\n", (long long)estimate);
        printf("JPY decimal places: %lld\n",
            (long long)xm_exponent("JPY", 3));
    }

    parts = xm_split(taxed, 3);
    if (parts == NULL || xrtValueType(parts) != XVALUE_ARRAY) {
        xrtValueRelease(parts);
        return 1;
    }
    for (i = 0; i < 3; ++i) {
        int64_t amount;
        char *text;
        if (!xrtValueGetInt(xrtValueArrayAt(parts, (int64)i), &amount)) {
            xrtValueRelease(parts);
            return 1;
        }
        text = (char *)xm_format(amount, "USD", 3);
        if (text == NULL) {
            xrtValueRelease(parts);
            return 1;
        }
        printf("friend %u pays %s\n", (unsigned)(i + 1), text);
        xrtFree(text);
    }
    xrtValueRelease(parts);

    for (i = 0; i < sizeof(currencies) / sizeof(currencies[0]); ++i) {
        const char *code = currencies[i];
        const char *symbol = xm_symbol(code, strlen(code));
        char *text = (char *)xm_format(123456, code, strlen(code));
        if (text == NULL) return 1;
        printf("%s %s %s\n", code, symbol, text);
        xrtFree(text);
    }
    puts("money example: PASS");
    return 0;
}
