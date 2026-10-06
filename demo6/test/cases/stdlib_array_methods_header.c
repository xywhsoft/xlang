#include "src/runtime/xl6_array_methods.h"
bool (*methodClear)(xvalue*) = xl6ArrayMethodClearV1;
int64_t (*methodCapacity)(const xvalue*) = xl6ArrayMethodCapacityV1;
bool (*methodInsert)(xvalue*,int64_t,const xvalue*) = xl6ArrayMethodInsertV1;
int main(void) { return 0; }
