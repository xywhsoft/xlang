#include "src/runtime/xl6_text_value.h"

static bool toTyped(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    if (!source || !target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (type != xl6CharTypeV1()) { xrtSetErrorKind(XERR_TYPE); return false; }
    uint32 value;
    if (!xrtValueGetChar(source,&value)) return false;
    *(uint32*)target=value; return true;
}
static xvalue* fromTyped(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (type != xl6CharTypeV1()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtValueChar(*(const uint32*)source);
}
XL6_TEXT_API const xvalueconverter* xl6CharConverterV1(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
