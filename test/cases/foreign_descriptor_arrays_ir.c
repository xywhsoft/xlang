#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m,const char* name)
{
 if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"accepted damaged descriptor Array IR: %s\n",name);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_DESCRIPTOR_ARRAY",NULL):xl6IrCgenEmitC(&out,m,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
 size_t embedded=0,standalone=0;
 for(size_t i=0;i<m->foreignArrayGroupCount;++i){XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i],saved=*row;
  if(!row->packPhysicalIndexPlusOne){++standalone;strcpy(row->packDataField,"items");refused(m,"standalone stray destination");*row=saved;continue;}
  ++embedded;
  row->packPhysicalIndexPlusOne=0;refused(m,"lost parent");*row=saved;
  row->packPhysicalIndexPlusOne=65;refused(m,"parent bounds");*row=saved;
  row->packPhysicalIndexPlusOne=64;refused(m,"missing parent");*row=saved;
  row->packDataField[0]=0;refused(m,"missing pointer field");*row=saved;
  row->packCountField[0]=0;refused(m,"missing count field");*row=saved;
  memset(row->packDataField,'x',sizeof(row->packDataField));refused(m,"unterminated pointer field");*row=saved;
  memset(row->packCountField,'x',sizeof(row->packCountField));refused(m,"unterminated count field");*row=saved;
  strcpy(row->packDataField,"items;boom");refused(m,"field injection");*row=saved;
  strcpy(row->packCountField,"ports..count");refused(m,"malformed path");*row=saved;
  strcpy(row->packCountField,row->packDataField);refused(m,"same destinations");*row=saved;
  snprintf(row->packCountField,sizeof(row->packCountField),"%s.child",row->packDataField);refused(m,"prefix destinations");*row=saved;
  strcpy(row->packDataField,"missing.items");refused(m,"missing nested record witness");*row=saved;
  row->recordCName[0]=0;refused(m,"nonrecord embedded group");*row=saved;
  row->arrayCount=0;refused(m,"zero columns");*row=saved;
  row->arrayCount=64;refused(m,"column bounds");*row=saved;
  ++row->sizeParameter;refused(m,"count position");*row=saved;
  row->firstParameter=63;refused(m,"logical range");*row=saved;
  row->countAbi=0;refused(m,"missing count ABI");*row=saved;
  XL6_IrNode* node=&m->nodes[row->declaration-1];uint16 flags=node->foreignCall.inputMappingFacts;
  node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_ARRAY_PACKS_PRESENT;refused(m,"presence bit lost");node->foreignCall.inputMappingFacts=flags;
  node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_PACKS_PRESENT;refused(m,"enclosing packs lost");node->foreignCall.inputMappingFacts=flags;
  node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_ARRAY_GROUPS_PRESENT;refused(m,"array bit lost");node->foreignCall.inputMappingFacts=flags;
  for(size_t j=0;j<i;++j){XL6_ForeignArrayGroupContract* old=&m->foreignArrayGroups[j];
   if(old->declaration!=row->declaration || old->packPhysicalIndexPlusOne!=row->packPhysicalIndexPlusOne)continue;
   strcpy(row->packDataField,old->packDataField);refused(m,"cross group field collision");*row=saved;
   row->physicalIndex=old->physicalIndex;refused(m,"duplicate group ID");*row=saved;
  }
  for(size_t f=0;f<m->foreignPackFieldCount;++f){XL6_ForeignPackFieldContract* field=&m->foreignPackFields[f];
   if(field->declaration!=row->declaration || field->packPhysicalIndex+1!=row->packPhysicalIndexPlusOne || field->kind!=XL6_FOREIGN_PACK_FIELD_VALUE)continue;
   strcpy(row->packDataField,field->field);refused(m,"ordinary field collision");*row=saved;
   uint32 parameter=field->parameter;field->parameter=row->firstParameter;refused(m,"array input reused by ordinary field");field->parameter=parameter;
  }
 }
 for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];for(size_t p=0;p<t->parameterCount;++p){
  const XL6_ForeignArrayGroupContract* row=xl6IrForeignArrayParameter(m,t,p);if(!row || !row->packPhysicalIndexPlusOne)continue;
  XL6_IrCallParameterPhysicalAbi physical=t->parameterPhysicalAbis[p];t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,"target physical");t->parameterPhysicalAbis[p]=physical;
  XL6_IrParameterAbi abi=t->parameterAbis[p];t->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(m,"target ownership");t->parameterAbis[p]=abi;
 }}
 size_t count=m->foreignArrayGroupCount;m->foreignArrayGroupCount=m->foreignArrayGroupCapacity+1;refused(m,"array storage bounds");m->foreignArrayGroupCount=count;
 assert(embedded>=7 && standalone && xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&unit);printf("Descriptor Array sealed IR mutations: %u\n",mutations);return 0;
}
