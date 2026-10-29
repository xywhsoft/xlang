#ifndef XL6_VALUE_ADMISSION_INTERNAL_H
#define XL6_VALUE_ADMISSION_INTERNAL_H
#include "xl6_text_value.h"

/* Private resident-only walk. The predicate/context are synchronous stack
 * borrows, never retained or generated-module callbacks. Keep one source for
 * primitive/nominal consuming validation and its destructive error boundary. */
typedef bool (*XL6_AdmissionPredicate)(const xvalue*, const void*);
static inline xvalue* xl6AdmissionFailure(xvalue* source, xvalueiter* iterator, xerrkind kind)
{
    if (!xrtGetError()) xrtSetErrorKind(kind);
    xerror* error=xrtTakeError();
    xrtValueIterDestroy(iterator); xrtValueRelease(source);
    xrtClearError(); xrtSetErrorTake(error);
    return NULL;
}
static inline xvalue* xl6AdmissionCollectionTake(xvalue* source, int container,
    XL6_AdmissionPredicate predicate, const void* context)
{
    if (container<XVALUE_ARRAY || container>XVALUE_OBJECT || !source)
        return xl6AdmissionFailure(source,NULL,XERR_ARGUMENT);
    if (xrtValueType(source)!=container) return xl6AdmissionFailure(source,NULL,XERR_TYPE);
    if (!predicate) return source;
    xvalueiter* iterator=xrtValueIterCreate(source);
    if (!iterator) return xl6AdmissionFailure(source,NULL,XERR_MEMORY);
    xvaluekey key={0}; xvalue* item=NULL; int step;
    while ((step=xrtValueIterAdvance(iterator,&key,&item))>0)
        if (!predicate(item,context)) return xl6AdmissionFailure(source,iterator,XERR_TYPE);
    if (step<0) return xl6AdmissionFailure(source,iterator,XERR_TYPE);
    xrtValueIterDestroy(iterator); return source;
}
#endif
