#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
 assert(!xl6IrVerifyModule(ir,NULL));
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_FIXED_REF",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 unsigned targets=0;
 for(size_t t=0;t<ir->callTargetCount;++t){
  XL6_IrCallTarget* target=&ir->callTargets[t];
  for(size_t p=0;p<target->parameterCount;++p){
   if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_C_AGGREGATE_BORROW)continue;
   XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&ir->types,target->parameterTypes[p]);
   if(type->kind!=XL6_IR_TYPE_C_ARRAY)continue;
   ++targets;XL6_IrType saved=*type;
   type->nativeArrayExtent=0;refused(ir);*type=saved;
   type->nativeArrayExtent=3;refused(ir);*type=saved;
   type->nativeArrayExtent=UINT64_MAX;refused(ir);*type=saved;
   type->params=NULL;refused(ir);*type=saved;
   type->paramCount=0;refused(ir);*type=saved;
   type->cAbi=XL6_IR_C_ABI_C_RECORD;refused(ir);*type=saved;
   type->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refused(ir);*type=saved;
   XL6_IrTypeId element=type->params[0];type->params[0]=0;refused(ir);type->params[0]=element;
   type->params[0]=target->parameterTypes[p];refused(ir);type->params[0]=element;
   XL6_IrNode* declaration=&ir->nodes[target->declaration-1];uint64 flags=declaration->foreignCall.constAggregateAddresses;
   declaration->foreignCall.constAggregateAddresses=0;refused(ir);declaration->foreignCall.constAggregateAddresses=flags;
   XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
   refused(ir);target->parameterPhysicalAbis[p]=abi;
   XL6_IrParameterAbi logical=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_BORROW;refused(ir);target->parameterAbis[p]=logical;
   const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(ir,element);
   if(layout && layout->fieldCount){
    XL6_IrAggregateField* field=&ir->aggregateFields[layout->firstField];XL6_IrTypeId fieldType=field->type;field->type=0;refused(ir);field->type=fieldType;
   }
  }
 }
 assert(targets>=3 && xl6IrVerifyModule(ir,NULL));printf("fixed refs: %u targets, %u mutations refused before C/header output\n",targets,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
