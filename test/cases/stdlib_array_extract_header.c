#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_array_extract.h"
static bool (*const boolean)(const xvalue*,uint64_t,uint64_t,int64_t,bool)=xl6ArrayExtractBoolV1;
static int64_t (*const integer)(const xvalue*,uint64_t,uint64_t,int64_t,bool)=xl6ArrayExtractI64V1;
static xstrview (*const text)(const xvalue*,uint64_t,uint64_t,int64_t,bool,bool)=xl6ArrayExtractStringV1;
static xvalue* (*const value)(const xvalue*,uint64_t,uint64_t,int64_t,bool)=xl6ArrayExtractValueV1;
static xvalue* (*const collection)(const xvalue*,unsigned int,int64_t,xstrview)=xl6CollectionExtractValueV1;
int main(void) { return !boolean || !integer || !text || !value || !collection; }
