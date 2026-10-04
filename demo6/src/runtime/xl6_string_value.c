#include "src/runtime/xl6_string_type.h"

/* Projection is a separate resident island from storage-only operations. */
static bool toTyped(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    if (!source || !target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (type != xl6StringTypeV1()) { xrtSetErrorKind(XERR_TYPE); return false; }
    xstrview view={0};
    if (!xrtValueGetString(source,&view)) return false;
    return xrtTypeCopyValue(type,target,&view);
}
static xvalue* fromTyped(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (type != xl6StringTypeV1()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    const xstrview* view=source;
    if (!view->Data) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtValueString(*view);
}
XL6_TEXT_API const xvalueconverter* xl6StringConverterV1(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
