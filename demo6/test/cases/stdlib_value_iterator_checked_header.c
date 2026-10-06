#include "src/runtime/xl6_value_iterator.h"
_Static_assert(_Generic(((XL6_ValueIteratorRuntimeV2*)0)->Advance,
    bool (*)(XL6_ValueIterator*,bool*):1,default:0),"Advance must separate status from EOF");
int main(void) {return 0;}
