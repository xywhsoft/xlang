#include <stdio.h>
#ifndef XL6_NATIVE_DEFINE
#error Missing actual transported C string definition
#endif
#ifdef XL6_RESPONSE_INTENTIONAL_ERROR
#error intentional-response-compile-failure
#endif
#ifdef XL6_RESPONSE_WARNING
#warning response-stderr-marker
#endif
#if defined(XL6_EXPECTED_PADDING) && XL6_RESPONSE_PADDING_3999 != 3999
#error Response arguments were truncated before their tail
#endif
int main(void)
{
    static const unsigned char value[] = XL6_NATIVE_DEFINE;
    size_t index;
    printf("%u:", (unsigned)(sizeof(value) - 1));
    for (index = 0; index + 1 < sizeof(value); ++index) printf("%02x", value[index]);
    puts("");
    return 0;
}
