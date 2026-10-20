/* IR/ABI verification only: never creates a socket or endpoint. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals,products,targets;
static void refused(XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(t)assert(!xl6IrRuntimeDirectBridgeTargetMatches(m,t));
    assert(!xl6IrVerifyModule(m,NULL));
    for(unsigned h=0;h<2;++h){XL6_Output o;XL6_DiagList d;size_t size=SIZE_MAX;
        xl6DiagInit(&d);assert(xl6OutputOpenMemory(&o));
        assert(!(h?xl6IrCgenEmitHeader(&o,m,"BAD_SOCKET",&d):xl6IrCgenEmitC(&o,m,&d)));
        char* text=xl6OutputTakeMemoryText(&o,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&o);xl6DiagFree(&d);}
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit u;xl6CompileUnitInit(&u);
    assert(xl6CompileUnitLoadFile(&u,argv[1],1)&&xl6IrLowerModule(&u.ir,&u.parse.diags));XL6_IrModule* m=&u.ir;
    assert(xl6IrVerifyModule(m,NULL));
    assert(m->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE&&m->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE);
    XL6_IrTypeId plain=XL6_IR_TYPE_INVALID;
    for(size_t i=0;i<m->types.count;++i){const XL6_IrType* t=&m->types.items[i];
        if(t->kind==XL6_IR_TYPE_BUILTIN&&t->cAbi==XL6_IR_C_ABI_I64&&t->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)plain=(XL6_IrTypeId)i+1;}
    assert(plain);
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];
        if(!t->cName||strncmp(t->cName,"xl6NetSocket",12))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(m,t));XL6_IrCallTarget saved=*t;++targets;
        t->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refused(m,t);*t=saved;
        t->effectAbi=XL6_IR_CALL_EFFECT_PURE;refused(m,t);*t=saved;
        t->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        refused(m,t);*t=saved;
        t->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;refused(m,t);*t=saved;
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE){t->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m,t);*t=saved;}
        if(saved.nativeHandlePolicyCName){t->nativeHandlePolicyCName="UnregisteredSocketPolicy";refused(m,NULL);*t=saved;}
        XL6_IrType* tuple=(XL6_IrType*)xl6IrTypeGet(&m->types,t->returnType);
        if(!tuple||tuple->nativeProductAbi!=XL6_IR_NATIVE_PRODUCT_I64U64_V1)continue;
        ++products;XL6_IrType tupleCopy=*tuple;XL6_IrTypeId status=tuple->params[0];
        tuple->params[0]=plain;
        assert(xl6IrNativeProductAbiForShape(&m->types,tuple)==XL6_IR_NATIVE_PRODUCT_I64U64_V1);
        refused(m,t);tuple->params[0]=status;
        tuple->nativeProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLU64_V1;refused(m,t);*tuple=tupleCopy;
        XL6_IrType* head=(XL6_IrType*)xl6IrTypeGet(&m->types,status);XL6_IrType copy=*head;
        head->nominalSymbol=0;refused(m,t);*head=copy;
        head->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32;refused(m,t);*head=copy;
        XL6_IrType* tail=(XL6_IrType*)xl6IrTypeGet(&m->types,tuple->params[1]);copy=*tail;
        tail->integerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32;refused(m,t);*tail=copy;
        XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(m,status);assert(layout);
        size_t n=layout->variantCount;--layout->variantCount;refused(m,t);layout->variantCount=n;
        XL6_IrEnumVariant* member=&m->enumVariants[layout->firstVariant+1];char* name=member->name;
        member->name="different";refused(m,t);member->name=name;
    }
    assert(targets==25&&products==7&&refusals>150&&xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&u);
    printf("Socket XLIR: %zu refusals, %zu direct targets, %zu nominal status/count products; no C/header on corrupt contracts\n",refusals,targets,products);return 0;
}
