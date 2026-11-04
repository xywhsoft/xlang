#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid packed IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_PACK",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);}
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 size_t count=ir->foreignPackCount,capacity=ir->foreignPackCapacity,fields=ir->foreignPackFieldCount,fieldCapacity=ir->foreignPackFieldCapacity;
 XL6_ForeignPackContract* rows=ir->foreignPacks;XL6_ForeignPackFieldContract* fieldRows=ir->foreignPackFields;assert(count>=6 && fields>=40);
 ir->foreignPackCount=capacity+1;refused(ir,"pack count");ir->foreignPackCount=count;
 ir->foreignPackCapacity=count-1;refused(ir,"pack capacity");ir->foreignPackCapacity=capacity;
 ir->foreignPacks=NULL;refused(ir,"pack rows");ir->foreignPacks=rows;
 ir->foreignPackCount=0;refused(ir,"pack presence");ir->foreignPackCount=count;
 ir->foreignPackFieldCount=fieldCapacity+1;refused(ir,"field count");ir->foreignPackFieldCount=fields;
 ir->foreignPackFields=NULL;refused(ir,"field rows");ir->foreignPackFields=fieldRows;
 ir->foreignPackFieldCount=0;refused(ir,"field presence");ir->foreignPackFieldCount=fields;
 for(size_t i=0;i<count;++i){XL6_ForeignPackContract* row=&rows[i];XL6_ForeignPackContract saved=*row;
  row->declaration=0;refused(ir,"declaration");*row=saved;
  row->physicalIndex=63;refused(ir,"physical");*row=saved;
  row->parameterCount=0;refused(ir,"empty group");*row=saved;
  row->firstParameter=63;row->parameterCount=2;refused(ir,"group overflow");*row=saved;
  row->header[0]='!';refused(ir,"header");*row=saved;
  row->recordCName[0]='!';refused(ir,"record identifier");*row=saved;
  XL6_IrNode* decl=&ir->nodes[saved.declaration-1];uint16 bits=decl->foreignCall.inputMappingFacts;
  decl->foreignCall.inputMappingFacts&=(uint16)~XL6_FOREIGN_INPUT_PACKS_PRESENT;refused(ir,"lost bit");decl->foreignCall.inputMappingFacts=bits;
  decl->foreignCall.inputMappingFacts|=512;refused(ir,"unknown bit");decl->foreignCall.inputMappingFacts=bits;
  if(i+1<count){XL6_ForeignPackContract next=rows[i+1];rows[i+1]=saved;refused(ir,"duplicate group");rows[i+1]=next;}
 }
 unsigned kinds=0;
 for(size_t i=0;i<fields;++i){XL6_ForeignPackFieldContract* row=&fieldRows[i];unsigned bit=1u<<row->kind;if(kinds&bit)continue;kinds|=bit;
  XL6_ForeignPackFieldContract saved=*row;
  row->packPhysicalIndex=63;refused(ir,"field pack");*row=saved;
  row->parameter=62;refused(ir,"source outside group");*row=saved;
  row->logicalType=UINT32_MAX;refused(ir,"logical type");*row=saved;
  row->physicalCName[0]='!';refused(ir,"physical type");*row=saved;
  row->kind=XL6_FOREIGN_PACK_FIELD_INVALID;refused(ir,"field mode");*row=saved;
  row->countAbi=(XL6_ForeignSpanCountAbi)99;refused(ir,"count type");*row=saved;
  row->field[0]='!';refused(ir,"field identifier");*row=saved;
  if(i+1<fields){XL6_ForeignPackFieldContract next=fieldRows[i+1];fieldRows[i+1]=saved;refused(ir,"duplicate field");fieldRows[i+1]=next;}
 }
 unsigned targets=0;
 for(size_t t=0;t<ir->callTargetCount;++t){XL6_IrCallTarget* target=&ir->callTargets[t];
  const XL6_IrNode* decl=xl6IrNodeGet(ir,target->declaration);if(!decl || !xl6ForeignInputHasPacks(decl->foreignCall.inputMappingFacts))continue;
  ++targets;XL6_IrCallVariadicAbi variadic=target->variadicAbi;target->variadicAbi=XL6_IR_CALL_VARIADIC_C;refused(ir,"varargs");target->variadicAbi=variadic;
  const XL6_ForeignPackContract* pack=xl6IrForeignPackParameter(ir,target,target->parameterCount>1 ? 1:0);if(!pack)pack=xl6IrForeignPackParameter(ir,target,0);assert(pack);
  size_t p=pack->firstParameter;XL6_IrTypeId type=target->parameterTypes[p];target->parameterTypes[p]=0;refused(ir,"target type");target->parameterTypes[p]=type;
  XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(ir,"target ownership");target->parameterAbis[p]=abi;
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW;refused(ir,"target physical");target->parameterPhysicalAbis[p]=physical;
 }
 for(size_t i=0;i<fields;++i){XL6_ForeignPackFieldContract* row=&fieldRows[i];XL6_IrCallTarget fake={0};fake.declaration=row->declaration;
  const XL6_ForeignPackContract* pack=xl6IrForeignPackGet(ir,&fake,row->packPhysicalIndex);
  if(!pack || strcmp(pack->recordCName,"PackSmall") || row->packPhysicalIndex)continue;
  XL6_ForeignPackFieldContract saved=*row;strcpy(row->field,"other");refused(ir,"same-C coherent field change");*row=saved;break;
 }
 assert(targets>=5 && xl6IrVerifyModule(ir,NULL));printf("pack: %zu schemas, %zu fields, %u targets, %u mutations refused before C/header output\n",count,fields,targets,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
