/* Instantiate one resident conversion island per physical profile. Keep this
 * out of the storage-only descriptor TU: typed Stack does not need Value
 * projection code. XRT owns numeric range/lossless conversion and xvalue
 * allocation; the adapter owns only nullable presence and char's exact kind. */
#include "xl6_text_value.h"

static bool toTyped(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    if (!source || !target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (type != XL6_NULLABLE_GET()) { xrtSetErrorKind(XERR_TYPE); return false; }
    XL6_NULLABLE_CARRIER item={0};
    if (xrtValueType(source)!=XVALUE_NULL) {
#if defined(XL6_NULLABLE_CHAR)
        if (!xrtValueGetChar(source,&item.value)) return false;
#else
        if (!xrtValueToTyped(source,XL6_NULLABLE_ELEMENT(),&item.value,NULL)) return false;
#endif
        item.hasValue=true;
    }
    *(XL6_NULLABLE_CARRIER*)target=item; return true;
}
static xvalue* fromTyped(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (type != XL6_NULLABLE_GET()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    const XL6_NULLABLE_CARRIER* item=source;
    if (!item->hasValue) return xrtValueNull();
#if defined(XL6_NULLABLE_CHAR)
    return xrtValueChar(item->value);
#else
    return xrtValueFromTyped(XL6_NULLABLE_ELEMENT(),&item->value,NULL);
#endif
}
XL6_TEXT_API const xvalueconverter* XL6_NULLABLE_CONVERTER(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
#undef XL6_NULLABLE_CARRIER
#undef XL6_NULLABLE_ELEMENT
#undef XL6_NULLABLE_GET
#undef XL6_NULLABLE_CONVERTER
#undef XL6_NULLABLE_CHAR
