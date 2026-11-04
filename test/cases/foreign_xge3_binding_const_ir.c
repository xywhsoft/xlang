#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid qualifier IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_QUALIFIER",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* data=xl6OutputTakeMemoryText(&out,&size);assert(data && !size);xrtFree(data);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 size_t count=ir->foreignPointerQualifierCount,capacity=ir->foreignPointerQualifierCapacity;
 XL6_ForeignPointerQualifierContract* rows=ir->foreignPointerQualifiers;assert(count>=4);
 ir->foreignPointerQualifierCount=capacity+1;refused(ir,"count beyond storage");ir->foreignPointerQualifierCount=count;
 ir->foreignPointerQualifierCapacity=count-1;refused(ir,"capacity");ir->foreignPointerQualifierCapacity=capacity;
 ir->foreignPointerQualifiers=NULL;refused(ir,"missing storage");ir->foreignPointerQualifiers=rows;
 ir->foreignPointerQualifierCount=0;refused(ir,"lost contracts");ir->foreignPointerQualifierCount=count;
 XL6_ForeignPointerQualifierContract first=rows[0],second=rows[1];
 rows[0].declaration=0;refused(ir,"declaration");rows[0]=first;
 rows[0].parameter=63;refused(ir,"parameter");rows[0]=first;
 strcpy(rows[0].cName,"ManagedChild*");refused(ir,"lost qualification");rows[0]=first;
 memset(rows[0].cName,'x',sizeof(rows[0].cName));refused(ir,"unterminated spelling");rows[0]=first;
 rows[1]=rows[0];refused(ir,"duplicate");rows[1]=second;
 unsigned managed=0,caller=0;
 for(size_t i=0;i<ir->callTargetCount;++i){
  XL6_IrCallTarget* target=&ir->callTargets[i];
  for(size_t p=0;p<target->parameterCount;++p){
   XL6_IrCallParameterPhysicalAbi saved=target->parameterPhysicalAbis[p];
   if(saved!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT && saved!=XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_FOREIGN_VIEW_BORROW)continue;
   caller+=saved==XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT;
   managed+=saved==XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_FOREIGN_VIEW_BORROW;
   target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"wrong physical ABI");target->parameterPhysicalAbis[p]=saved;
   XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];target->parameterPhysicalFieldDeclarations[p]=1;refused(ir,"unexpected field projection");target->parameterPhysicalFieldDeclarations[p]=field;
   assert(xl6IrVerifyModule(ir,NULL));
  }
 }
 assert(managed && caller);printf("native pointer const: %u managed, %u caller targets, %u invalid mutations refused before C/header output\n",managed,caller,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
