#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid Region IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output output;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
  assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"BAD_REGION",NULL):xl6IrCgenEmitC(&output,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&output,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&output);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 size_t count=ir->foreignRegionCount,capacity=ir->foreignRegionCapacity;assert(count>=6);
 XL6_ForeignRegionContract* rows=ir->foreignRegions;
 ir->foreignRegionCount=capacity+1;refused(ir,"count/capacity");ir->foreignRegionCount=count;
 ir->foreignRegionCapacity=count-1;refused(ir,"capacity");ir->foreignRegionCapacity=capacity;
 ir->foreignRegions=NULL;refused(ir,"missing rows");ir->foreignRegions=rows;
 ir->foreignRegionCount=0;refused(ir,"lost rows");ir->foreignRegionCount=count;
 XL6_ForeignRegionContract first=rows[0],second=rows[1];
 rows[0].declaration=0;refused(ir,"declaration");rows[0]=first;
 rows[0].physicalIndex=63;refused(ir,"physical input");rows[0]=first;
 rows[0].parameter=63;refused(ir,"parameter");rows[0]=first;
 rows[0].logicalType=0;refused(ir,"logical type");rows[0]=first;
 rows[0].unitBytes=0;refused(ir,"zero unit");rows[0]=first;
 rows[0].unitBytes=UINT32_MAX;refused(ir,"unit range");rows[0]=first;
 rows[0].access=XL6_FOREIGN_REGION_ACCESS_INVALID;refused(ir,"unknown access");rows[0]=first;
 rows[0].nullable=2;refused(ir,"nullable range");rows[0]=first;
 rows[0].nullable^=1;refused(ir,"nullable type");rows[0]=first;
 rows[1]=rows[0];refused(ir,"duplicate physical input");rows[1]=second;
 for(size_t r=0;r<count;++r){
  XL6_ForeignRegionContract original=rows[r];XL6_ForeignRegionSelector* selectors[]={&rows[r].width,&rows[r].height,&rows[r].stride};
  for(size_t s=0;s<3;++s){
   XL6_ForeignRegionSelector* row=selectors[s],saved=*row;
   row->kind=XL6_FOREIGN_REGION_SELECTOR_INVALID;refused(ir,"selector kind");*row=saved;
   row->fieldType=0;refused(ir,"selector type");*row=saved;
   row->parameter=63;refused(ir,"selector parameter");*row=saved;
   if(row->kind==XL6_FOREIGN_REGION_SELECTOR_INPUT){
    row->physicalIndex=original.physicalIndex;refused(ir,"selector data pointer");*row=saved;
    strcpy(row->field,"injected");refused(ir,"scalar field");*row=saved;
   }else{
    row->recordType=0;refused(ir,"selector record");*row=saved;
    row->fieldDeclaration=0;refused(ir,"selector member declaration");*row=saved;
    strcpy(row->field,"missing");refused(ir,"selector member");*row=saved;
   }
  }
  XL6_IrNode* declaration=&ir->nodes[original.declaration-1];uint16 facts=declaration->foreignCall.inputMappingFacts;
  assert(xl6ForeignInputHasRegions(facts));
  declaration->foreignCall.inputMappingFacts=(uint8)(facts&127u);refused(ir,"lost Region presence");declaration->foreignCall.inputMappingFacts=facts;
  declaration->foreignCall.inputMappingFacts=255;refused(ir,"unknown callback count");declaration->foreignCall.inputMappingFacts=facts;
  uint16 changed;assert(xl6ForeignInputMappingsSet(xl6ForeignInputCallbackCount(facts)+1,1,0,&changed));
  declaration->foreignCall.inputMappingFacts=changed;refused(ir,"callback count plus Region");declaration->foreignCall.inputMappingFacts=facts;
  if(xl6ForeignInputCallbackCount(facts)){
   assert(xl6ForeignInputMappingsSet(0,1,0,&changed));declaration->foreignCall.inputMappingFacts=changed;
   refused(ir,"lost callback span with Region");declaration->foreignCall.inputMappingFacts=facts;
  }
 }
 size_t observerCount=ir->foreignRegionObserverCount,observerCapacity=ir->foreignRegionObserverCapacity;assert(observerCount==1);
 XL6_ForeignRegionObserverContract* observerRows=ir->foreignRegionObservers;XL6_ForeignRegionObserverContract observed=observerRows[0];
 ir->foreignRegionObserverCount=observerCapacity+1;refused(ir,"observer count/capacity");ir->foreignRegionObserverCount=observerCount;
 ir->foreignRegionObservers=NULL;refused(ir,"missing observer storage");ir->foreignRegionObservers=observerRows;
 ir->foreignRegionObserverCount=0;refused(ir,"lost observer");ir->foreignRegionObserverCount=observerCount;
 observerRows[0].querySymbol=0;refused(ir,"observer symbol");observerRows[0]=observed;
 observerRows[0].queryDeclaration=0;refused(ir,"observer declaration");observerRows[0]=observed;
 observerRows[0].recordType=0;refused(ir,"observer output type");observerRows[0]=observed;
 observerRows[0].regionPhysicalIndex=63;refused(ir,"observer owner input");observerRows[0]=observed;
 observerRows[0].parameters[0]=63;refused(ir,"observer source parameter");observerRows[0]=observed;
 observerRows[0].physicalInputs[0]=observed.regionPhysicalIndex;refused(ir,"observer data dependency");observerRows[0]=observed;
 observerRows[0].inputCount=0;refused(ir,"observer argument count");observerRows[0]=observed;
 observerRows[0].parameters[3]=1;refused(ir,"observer unused input");observerRows[0]=observed;
 XL6_IrNode* query=&ir->nodes[observed.queryDeclaration-1];uint32 flags=query->flags;
 query->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(ir,"observer effect");query->flags=flags;
 for(size_t t=0;t<ir->callTargetCount;++t){
  XL6_IrCallTarget* target=&ir->callTargets[t];
  for(size_t p=0;p<target->parameterCount;++p)if(xl6IrForeignRegionGet(ir,target,p)){
   XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
   refused(ir,"target physical pointer");target->parameterPhysicalAbis[p]=physical;
   XL6_IrParameterAbi abi=target->parameterAbis[p];target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(ir,"target borrow");target->parameterAbis[p]=abi;
   XL6_IrTypeId type=target->parameterTypes[p];target->parameterTypes[p]=0;refused(ir,"target type");target->parameterTypes[p]=type;
  }
 }
 unsigned overloads=0;
 for(size_t t=0;t<ir->callTargetCount;++t){
  XL6_IrCallTarget* target=&ir->callTargets[t];
  for(size_t p=0;p<target->parameterCount;++p){
   XL6_ForeignRegionContract* row=(XL6_ForeignRegionContract*)xl6IrForeignRegionGet(ir,target,p);
   if(!row || !xl6IrForeignRegionBufferField(ir,target,p) || row->access!=XL6_FOREIGN_REGION_ACCESS_READ)continue;
   XL6_ForeignRegionContract original=*row;
   ++row->unitBytes;refused(ir,"individually valid overload unit disagreement");*row=original;
   row->access=XL6_FOREIGN_REGION_ACCESS_WRITE;refused(ir,"individually valid overload constness disagreement");*row=original;
   if(row->width.kind==XL6_FOREIGN_REGION_SELECTOR_INPUT_FIELD){
    row->width=row->height;refused(ir,"individually valid overload field selector disagreement");*row=original;
   }
   ++overloads;
  }
 }
 assert(overloads>=2 && xl6IrVerifyModule(ir,NULL));printf("Region: %lu rows, %u overloads, %u mutations refused before C/header output\n",(unsigned long)count,overloads,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
