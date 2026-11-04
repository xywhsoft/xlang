#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid span variant: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SPAN_VARIANT",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);}
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned spans=0;
 for(size_t i=0;i<ir->foreignParameterCount;++i){XL6_ForeignParameterContract* row=&ir->foreignParameters[i];if(row->kind!=XL6_FOREIGN_PARAMETER_SPAN)continue;
  ++spans;XL6_ForeignParameterContract saved=*row;
  row->readOnly=0;strcpy(row->pointerCName,"float*");refused(ir,"coherent writable substitution");*row=saved;
  row->countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;refused(ir,"count ABI");*row=saved;
  strcpy(row->pointerCName,"const double*");strcpy(row->elementCName,"double");refused(ir,"coherent element type");*row=saved;
  strcpy(row->header,"other.h");refused(ir,"header");*row=saved;
 }
 for(size_t i=0;i<ir->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract* row=&ir->foreignPackFields[i];XL6_ForeignPackFieldContract saved=*row;
  row->logicalType=0;refused(ir,"pack logical type");*row=saved;
  strcpy(row->field,"other");refused(ir,"coherent pack projection mismatch");*row=saved;
  row->countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;refused(ir,"pack count ABI");*row=saved;
 }
 unsigned targets=0;
 for(size_t i=0;i<ir->callTargetCount;++i){XL6_IrCallTarget* target=&ir->callTargets[i];
  for(size_t p=0;p<target->parameterCount;++p){if(!xl6IrForeignParameterGet(ir,target,p))continue;++targets;
   XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"physical preparation");target->parameterPhysicalAbis[p]=physical;
   XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(ir,"ownership");target->parameterAbis[p]=abi;
   XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];target->parameterPhysicalFieldDeclarations[p]=field ? 0:target->declaration;refused(ir,"scope storage field");target->parameterPhysicalFieldDeclarations[p]=field;
   XL6_IrTypeId type=target->parameterTypes[p];target->parameterTypes[p]=0;refused(ir,"source type");target->parameterTypes[p]=type;
  }
 }
 assert(spans>=6 && targets>=6 && xl6IrVerifyModule(ir,NULL));printf("readonly span variants: %u schemas, %u targets, %u mutations refused before C/header output\n",spans,targets,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
