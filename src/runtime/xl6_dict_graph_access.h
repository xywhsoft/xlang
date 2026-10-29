#ifndef XL6_DICT_GRAPH_ACCESS_H
#define XL6_DICT_GRAPH_ACCESS_H
#include "xl6_dict.h"
/* Private resident view while XRT guards the Value shell. Begin authenticates
 * ops BEFORE payload access and owns a native operation pin. Never fabricate,
 * retain/copy the view, reacquire the guarded shell or use it after End. */
typedef struct XL6_DictGraphViewV1 {
    void* owner;
    XL6_DictLayoutV1 layout;
    xtypeddict* storage;
} XL6_DictGraphViewV1;
XL6_TEXT_API int xl6DictGraphBeginV1(const xvaluehandleops*,ptr,ptr,uint64_t,XL6_DictGraphViewV1*);
XL6_TEXT_API void xl6DictGraphEndV1(XL6_DictGraphViewV1*);
XL6_TEXT_API xvalue* xl6DictGraphEmptyV1(const XL6_DictGraphViewV1*);
/* Non-reference only. Reference children go directly from pinned real slots
 * into Next's single memo, without retaining an active source ancestor. */
XL6_TEXT_API xvalue* xl6DictGraphProjectSlotV1(const XL6_DictGraphViewV1*,const void*);
#endif
