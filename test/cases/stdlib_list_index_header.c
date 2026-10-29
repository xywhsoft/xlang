#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_value_index_read.h"
#include "src/runtime/xl6_value_index_write.h"
#include "src/runtime/xl6_array_value_read.h"
#include "src/runtime/xl6_array_value_write.h"
#include "src/runtime/xl6_array_methods.h"
#if defined(XRT_API) || defined(XRUNTIME_SINGLE_HEADER_H)
#error These headers must be standalone declarations, not XRT implementations
#endif
_Static_assert(_Generic(&xl6ListValueReadCopyV1,xvalue* (*)(const xvalue*,int64_t):1,default:0),"owned List read ABI");
_Static_assert(_Generic(&xl6ValueIndexReadCopyV1,xvalue* (*)(const xvalue*,const xvalue*):1,default:0),"owned dynamic read ABI");
_Static_assert(_Generic(&xl6ValueIndexSetTakeV1,bool (*)(xvalue*,const xvalue*,xvalue*):1,default:0),"consuming dynamic write ABI");
int main(void) { return 0; }
