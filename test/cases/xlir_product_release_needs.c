/* Focused private implementation unit. Abstract ABI descriptors only, no fake
 * native object/ownership graph and no claim of a complete compiler product. */
#include "src/cgen/xl6_ir_cgen.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    XL6_IrModule module={0}; XL6_IrType types[14]={0};
    module.types.items=types; module.types.count=module.types.capacity=14;
    const XL6_IrCAbi abis[]={XL6_IR_C_ABI_MANAGED_STRING,XL6_IR_C_ABI_C_STRING,
        XL6_IR_C_ABI_C_BYTES,XL6_IR_C_ABI_MANAGED_BYTES,XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
        XL6_IR_C_ABI_RUNTIME_ERROR_PTR,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,
        XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR,XL6_IR_C_ABI_CLOSURE_PTR,
        XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR};
    for(size_t n=0;n<sizeof(abis)/sizeof(*abis);++n) {
        types[n].kind=XL6_IR_TYPE_BUILTIN;types[n].cAbi=abis[n];
        types[n].ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;
        XL6_IrCgenNeeds needs={0};
        assert(xl6IrCgenStorageReleaseFunction(&types[n]));
        assert(xl6IrCgenAnalyzeReleaseNeeds(&module,(XL6_IrTypeId)(n+1),&needs,0));
        if(n==0)assert(needs.needsStringRuntime && !needs.needsFutureRuntime && !needs.needsClosureRuntime);
        if(n==1 || n==2)assert(needs.needsCOwnedRuntime);
        if(n==3)assert(needs.needsBytesRuntime && needs.needsXValueRuntime);
        if(n==4)assert(needs.needsXValueRuntime);
        if(n==5)assert(needs.needsErrorRuntime);
        if(n==6)assert(needs.needsFutureRuntime);
        if(n==7)assert(needs.needsGeneratorRuntime);
        if(n==8)assert(needs.needsClosureRuntime);
        if(n==9)assert(needs.needsXValueIteratorRuntime && needs.needsRuntimeTrap);
    }
    types[10].kind=XL6_IR_TYPE_BUILTIN;types[10].cAbi=XL6_IR_C_ABI_I64;
    types[10].ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    XL6_IrTypeId inner[]={1,7,11},outer[]={12,4,9,10};
    types[11].kind=types[12].kind=XL6_IR_TYPE_TUPLE;
    types[11].cAbi=types[12].cAbi=XL6_IR_C_ABI_PRODUCT;
    types[11].params=inner;types[11].paramCount=3;
    types[12].params=outer;types[12].paramCount=4;
    XL6_IrCgenNeeds needs={0};
    assert(xl6IrCgenAnalyzeReleaseNeeds(&module,13,&needs,0));
    assert(needs.needsStringRuntime && needs.needsFutureRuntime && needs.needsBytesRuntime &&
        needs.needsClosureRuntime && needs.needsXValueIteratorRuntime && !needs.needsGeneratorRuntime);
    XL6_IrTypeId cycle[]={14};types[13].kind=XL6_IR_TYPE_TUPLE;
    types[13].cAbi=XL6_IR_C_ABI_PRODUCT;types[13].params=cycle;types[13].paramCount=1;
    assert(!xl6IrCgenAnalyzeReleaseNeeds(&module,14,&needs,0));
    assert(!xl6IrCgenAnalyzeReleaseNeeds(&module,0,&needs,0));
    types[13].kind=XL6_IR_TYPE_BUILTIN;types[13].cAbi=XL6_IR_C_ABI_RAW_POINTER;
    types[13].ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;
    assert(!xl6IrCgenAnalyzeReleaseNeeds(&module,14,&needs,0));
    puts("physical release needs: 10 owned scalar ABIs, nested products, minimal flags, invalid/cyclic carrier refusal");
    return 0;
}
