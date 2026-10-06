#include "src/runtime/xl6_array_string_read.h"
#include "src/runtime/xl6_array_value_write.h"
_Static_assert(_Generic(&xl6ArrayReadStringV1,
    xstrview (*)(const xvalue*,uint64_t,uint64_t,int64_t):1,default:0),"Owned string ABI");
_Static_assert(_Generic(&xl6ArrayValueAppendTakeV1,
    bool (*)(xvalue*,xvalue*):1,default:0),"Consume append ABI");
_Static_assert(_Generic(&xl6ArrayValueSetTakeV1,
    bool (*)(xvalue*,int64_t,xvalue*):1,default:0),"Consume set ABI");
_Static_assert(_Generic(&xl6ValueIndexSetTakeV1,
    bool (*)(xvalue*,const xvalue*,xvalue*):1,default:0),"Consume dynamic index ABI");
int main(void) { return 0; }
