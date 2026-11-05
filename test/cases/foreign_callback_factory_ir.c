#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"invalid callback factory accepted: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_CALLBACK_FACTORY",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned found=0;
 for(size_t i=0;i<ir->callTargetCount;++i){
  XL6_IrCallTarget* target=&ir->callTargets[i];
  if(!target->cName || strcmp(target->cName,"callback_factory_share"))continue;
  ++found;assert(target->parameterCount==1 && target->parameterTypes && target->parameterPhysicalAbis);
  XL6_IrNode* declaration=&ir->nodes[target->declaration-1];XL6_ForeignCallFacts facts=declaration->foreignCall;
  XL6_IrType* receiver=(XL6_IrType*)xl6IrTypeGet(&ir->types,target->parameterTypes[0]);
  XL6_IrType* result=(XL6_IrType*)xl6IrTypeGet(&ir->types,target->returnType);
  assert(receiver && receiver->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK && result && result->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED);
  XL6_ForeignLifetimeAbi lifetime=receiver->foreignLifetime;
  receiver->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(ir,"raw caller receiver");receiver->foreignLifetime=lifetime;
  char* policy=result->foreignPolicy;result->foreignPolicy=NULL;refused(ir,"missing independent root policy");result->foreignPolicy=policy;
  lifetime=result->foreignLifetime;result->foreignLifetime=XL6_FOREIGN_LIFETIME_OWNER;refused(ir,"owner child result");result->foreignLifetime=lifetime;
  declaration->foreignCall.resultOwnerIndexPlusOne=1;refused(ir,"same epoch owner result");declaration->foreignCall=facts;
  declaration->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE;
  declaration->foreignCall.resultOwnerIndexPlusOne=1;refused(ir,"borrow instead of new native owner");declaration->foreignCall=facts;
  declaration->foreignCall.inplaceOutput=XL6_FOREIGN_INPLACE_STATUS;refused(ir,"compiler record from epoch");declaration->foreignCall=facts;
  declaration->foreignCall.outputIndexPlusOne=0;refused(ir,"missing physical output");declaration->foreignCall=facts;
  declaration->foreignCall.rollbackSymbol=0;refused(ir,"missing rollback");declaration->foreignCall=facts;
  uint64 flags=declaration->flags;declaration->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(ir,"raising native factory");declaration->flags=flags;
  declaration->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(ir,"pure owning producer");declaration->flags=flags;
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[0];
  target->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"raw pointer instead of callback view");target->parameterPhysicalAbis[0]=physical;
  XL6_IrCallOutputResultAbi output=target->outputResultAbi;target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_NONE;refused(ir,"missing status result");target->outputResultAbi=output;
 }
 assert(found && xl6IrVerifyModule(ir,NULL));
 printf("%u callback owning factory targets: %u invalid IR mutations, zero C/header output\n",found,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
