#include "xl6_net_frame_internal.h"
#include "xl6_net_buffer_abi.h"
_Static_assert(offsetof(XL6_NetLineFramerV1,input)==0&&offsetof(XL6_NetLengthFramerV1,input)==0,
    "Framer input must remain the first native member");
void xl6NetLineFramerAppendRefV1(void* raw,void* bytes,int64_t size,int64_t offset)
{xl6NetBufferAppendRefV1(&((XL6_NetLineFramerV1*)raw)->input,bytes,size,offset);}
void xl6NetLengthFramerAppendRefV1(void* raw,void* bytes,int64_t size,int64_t offset)
{xl6NetBufferAppendRefV1(&((XL6_NetLengthFramerV1*)raw)->input,bytes,size,offset);}
