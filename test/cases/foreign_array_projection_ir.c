#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* name)
{
 if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted damaged Array IR: %s\n",name);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_ARRAY",NULL):xl6IrCgenEmitC(&out,module,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
 assert(m->foreignArrayGroupCount>=5 && m->foreignArrayInputCount && m->foreignArrayFieldCount);
 for(size_t i=0;i<m->foreignArrayGroupCount;++i){XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i],saved=*row;
  row->declaration=0;refused(m,"group declaration");*row=saved;
  row->physicalIndex=63;refused(m,"physical slot");*row=saved;
  row->firstParameter=63;refused(m,"first column");*row=saved;
  row->arrayCount=0;refused(m,"zero count");*row=saved;
  row->arrayCount=64;refused(m,"count overflow");*row=saved;
  ++row->sizeParameter;refused(m,"size relation");*row=saved;
  row->countAbi=0;refused(m,"count ABI");*row=saved;
  memset(row->header,'x',sizeof(row->header));refused(m,"unterminated header");*row=saved;
  row->recordCName[0]=';';refused(m,"record injection");*row=saved;
  XL6_IrNode* node=&m->nodes[row->declaration-1];uint16_t bits=node->foreignCall.inputMappingFacts;
  node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_ARRAY_GROUPS_PRESENT;refused(m,"lost presence");node->foreignCall.inputMappingFacts=bits;
 }
 for(size_t i=0;i<m->foreignArrayInputCount;++i){XL6_ForeignArrayInputContract* row=&m->foreignArrayInputs[i],saved=*row;
  row->arrayType=0;refused(m,"array type");*row=saved;
  row->elementType=0;refused(m,"element type");*row=saved;
  row->column=63;refused(m,"column bounds");*row=saved;
  row->profile=0;refused(m,"profile missing");*row=saved;
  row->profile=(XL6_ForeignArrayProfile)99;refused(m,"unknown profile");*row=saved;
  row->kind=0;refused(m,"element kind");*row=saved;
  strcpy(row->elementCName,"void*");refused(m,"element physical ABI");*row=saved;
  memset(row->pointerCName,'x',sizeof(row->pointerCName));refused(m,"unterminated pointer");*row=saved;
 }
 for(size_t i=0;i<m->foreignArrayFieldCount;++i){XL6_ForeignArrayFieldContract* row=&m->foreignArrayFields[i],saved=*row;
  row->arrayType=0;refused(m,"record array type");*row=saved;
  row->elementType=0;refused(m,"record element type");*row=saved;
  row->profile=0;refused(m,"record profile");*row=saved;
  strcpy(row->physicalCName,"void*");refused(m,"record field ABI");*row=saved;
  row->field[0]=';';refused(m,"field injection");*row=saved;
 }
 for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* target=&m->callTargets[i];for(size_t p=0;p<target->parameterCount;++p){
  if(!xl6IrForeignArrayParameter(m,target,p))continue;
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,"target physical");target->parameterPhysicalAbis[p]=physical;
  XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(m,"target ownership");target->parameterAbis[p]=abi;
 }}
 size_t count=m->foreignArrayGroupCount;m->foreignArrayGroupCount=m->foreignArrayGroupCapacity+1;refused(m,"row storage");m->foreignArrayGroupCount=count;
 assert(xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&unit);printf("Array projection sealed IR mutations: %u\n",mutations);return 0;
}
