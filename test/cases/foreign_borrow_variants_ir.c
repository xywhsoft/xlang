#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned count;
static void refused(XL6_IrModule* ir)
{
 assert(!xl6IrVerifyModule(ir,NULL));
 for(unsigned header=0;header<2;++header){XL6_Output output;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
  assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"BAD_BORROW",NULL):xl6IrCgenEmitC(&output,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&output,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&output);}
 ++count;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned inputs=0;
 for(size_t t=0;t<ir->callTargetCount;++t){XL6_IrCallTarget* target=&ir->callTargets[t];
  if(!target->cName || strcmp(target->cName,"variant_take"))continue;
  for(size_t p=0;p<target->parameterCount;++p){XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&ir->types,target->parameterTypes[p]);
   if(!type || type->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK)continue;
   ++inputs;XL6_ForeignLifetimeAbi lifetime=type->foreignLifetime;type->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(ir);type->foreignLifetime=lifetime;
   char name[sizeof(type->cName)];memcpy(name,type->cName,sizeof(name));strcpy(type->cName,"const BorrowRoot*");refused(ir);memcpy(type->cName,name,sizeof(name));
   char* header=type->foreignHeader;type->foreignHeader="other.h";refused(ir);type->foreignHeader=header;
   XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir);target->parameterPhysicalAbis[p]=physical;
   XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(ir);target->parameterAbis[p]=abi;
  }
 }
 assert(inputs>=2 && xl6IrVerifyModule(ir,NULL));printf("borrow variants: %u inputs, %u mutations refused before C/header output\n",inputs,count);xl6CompileUnitFree(&unit);return 0;
}
