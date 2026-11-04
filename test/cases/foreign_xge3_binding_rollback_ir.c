#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid rollback IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_CHECKED_ROLLBACK",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* data=xl6OutputTakeMemoryText(&out,&size);assert(data && !size);xrtFree(data);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 unsigned helpers=0,factories=0;
 for(size_t i=0;i<ir->callTargetCount;++i){
  XL6_IrCallTarget* target=&ir->callTargets[i];XL6_IrNode* node=&ir->nodes[target->declaration-1];
  if(node->foreignCall.outputIndexPlusOne){
   ++factories;XL6_SymbolId saved=node->foreignCall.rollbackSymbol;
   node->foreignCall.rollbackSymbol=target->symbol;refused(ir,"recursive rollback");node->foreignCall.rollbackSymbol=saved;
  }
  if(target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_INT_STATUS || target->parameterCount!=1)continue;
  ++helpers;XL6_IrCallTarget saved=*target;XL6_IrNode source=*node;
  target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;refused(ir,"lost status prototype");*target=saved;
  target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(ir,"unchecked cleanup status");*target=saved;
  target->nativeSuccessCode+=1;refused(ir,"physical success mismatch");*target=saved;
  node->foreignCall.successCode+=1;refused(ir,"source success mismatch");*node=source;
  node->foreignCall.successCode+=1;target->nativeSuccessCode+=1;assert(xl6IrVerifyModule(ir,NULL));*node=source;*target=saved;
  node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(ir,"missing native no-raise contract");*node=source;
  node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS;refused(ir,"boolean cleanup");*node=source;
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[0];target->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"raw input projection");target->parameterPhysicalAbis[0]=physical;
  XL6_IrParameterAbi ownership=target->parameterAbis[0];target->parameterAbis[0]=XL6_IR_PARAMETER_ABI_OWNED;refused(ir,"input ownership transfer");target->parameterAbis[0]=ownership;
  assert(xl6IrVerifyModule(ir,NULL));
 }
 assert(factories>=2 && helpers>=2);printf("checked rollback: %u factories, %u helpers, %u mutations refused before C/header output\n",factories,helpers,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
