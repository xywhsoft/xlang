#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#ifndef XL6_POINT_SEQUENCE_GENERATED
#error Define XL6_POINT_SEQUENCE_GENERATED to the emitted C fixture
#endif
#include XL6_POINT_SEQUENCE_GENERATED
#ifndef XL6_POINT_SEQUENCE_PROBE
#error Define XL6_POINT_SEQUENCE_PROBE to the verified fixture probe function
#endif
static int leftMarker, rightMarker, rawMarker;
int main(void)
{
    assert(xlir_module_init()==1);
    xlir_result_i64 result=XL6_POINT_SEQUENCE_PROBE(&leftMarker,&rightMarker,&rawMarker);
    assert(result.ok && result.value==0);
    assert(!xrtGetError());
    puts("point Sequence: actual distinct static pointers, null defaults, Array/Stack/List/Dict and exact projection");
    return 0;
}
