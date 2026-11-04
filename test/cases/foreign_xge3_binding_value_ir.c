#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid borrowed value IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_BORROWED_VALUE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* data=xl6OutputTakeMemoryText(&out,&size);assert(data && !size);xrtFree(data);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 unsigned inputs=0,outputs=0;
 for(size_t i=0;i<ir->callTargetCount;++i){
  XL6_IrCallTarget* target=&ir->callTargets[i];
  for(size_t p=0;p<target->parameterCount;++p){
   if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_XVALUE_BORROW)continue;
   ++inputs;XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
   target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"mutable physical input");target->parameterPhysicalAbis[p]=physical;
   target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT;refused(ir,"not a caller view");target->parameterPhysicalAbis[p]=physical;
   XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(ir,"input ownership transfer");target->parameterAbis[p]=abi;
   XL6_ForeignPointerQualifierContract* row=(XL6_ForeignPointerQualifierContract*)xl6IrForeignPointerQualifierGet(ir,target,p);assert(row);
   XL6_ForeignPointerQualifierContract saved=*row;strcpy(row->cName,"xvalue*");refused(ir,"lost const");*row=saved;
   strcpy(row->cName,"const void*");refused(ir,"erased pointee");*row=saved;
  }
  if(target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_CONST_XVALUE_POINTER)continue;
  ++outputs;XL6_IrCallTarget saved=*target;XL6_IrNode* node=&ir->nodes[target->declaration-1];XL6_IrNode source=*node;
  target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;refused(ir,"mutable result prototype");*target=saved;
  target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(ir,"borrowed result escape");*target=saved;
  target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE;refused(ir,"class handle reinterpretation");*target=saved;
  node->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_NONE;refused(ir,"lost result fact");*node=source;
  node->foreignCall.outputIndexPlusOne=1;refused(ir,"output overlap");*node=source;
  node->flags|=XL6_AST_FLAG_FOREIGN_FIELD;refused(ir,"field projection");*node=source;
  assert(xl6IrVerifyModule(ir,NULL));
 }
 assert(inputs>=2 && outputs>=3);printf("borrowed var: %u inputs, %u result targets, %u mutations refused before C/header output\n",inputs,outputs,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
