#include <stdint.h>

typedef union XL_Union_____xl6__import__union__api__Payload {
    int64_t i;
    double d;
} XL_Union_____xl6__import__union__api__Payload;

XL_Union_____xl6__import__union__api__Payload xl6_union_api_default = {42};

XL_Union_____xl6__import__union__api__Payload xl6_union_api_make(int64_t value)
{
    XL_Union_____xl6__import__union__api__Payload payload;
    payload.i = value;
    return payload;
}

int64_t xl6_union_api_pick(XL_Union_____xl6__import__union__api__Payload payload)
{
    return payload.i;
}
