#ifndef XL6_DICT_METHODS_H
#define XL6_DICT_METHODS_H
#include "xl6_text_value.h"
/* Borrow receivers/keys; owned Value results. Native owners are authenticated
 * by the resident Dict provider. OBJECT is an explicit wire input; a refused
 * private native owner is never reinterpreted as wire. Missing keys are clean.
 * Failure is latched even when allocating a diagnostic is impossible. */
XL6_TEXT_API int64_t xl6DictValueCountV1(const xvalue*);
XL6_TEXT_API int64_t xl6DictValueCapacityV1(const xvalue*);
XL6_TEXT_API bool xl6DictValueCountIfPresentV1(const xvalue*,int64_t*);
XL6_TEXT_API xvalue* xl6DictValueCountOrNullV1(const xvalue*);
XL6_TEXT_API bool xl6DictValueHasV1(const xvalue*,const xvalue*);
XL6_TEXT_API bool xl6DictValueRemoveV1(const xvalue*,xstrview);
XL6_TEXT_API bool xl6DictValueClearV1(const xvalue*);
XL6_TEXT_API bool xl6DictValueReserveV1(const xvalue*,int64_t);
XL6_TEXT_API bool xl6DictValueTrimV1(const xvalue*);
XL6_TEXT_API xvalue* xl6DictValueTakeV1(const xvalue*,xstrview);
XL6_TEXT_API xvalue* xl6DictValueCloneV1(const xvalue*);
/* Independent result; inputs untouched, including every allocation failure.
 * Native/native merge uses complete layout + policy and atomic typed merge,
 * never a temporary wire map. Mixed wire input is admitted explicitly. */
XL6_TEXT_API xvalue* xl6DictValueMergeRightV1(const xvalue*,const xvalue*);
XL6_TEXT_API xvalue* xl6DictValueWithoutKeyV1(const xvalue*,xstrview);
#endif
