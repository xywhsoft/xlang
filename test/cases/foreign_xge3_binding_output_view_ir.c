#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid output-view IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_OUTPUT_VIEW",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 size_t count=ir->foreignOutputViewCount,capacity=ir->foreignOutputViewCapacity;assert(count>=9);
 XL6_ForeignOutputViewContract* rows=ir->foreignOutputViews;
 ir->foreignOutputViewCount=capacity+1;refused(ir,"count/capacity");ir->foreignOutputViewCount=count;
 ir->foreignOutputViewCapacity=count-1;refused(ir,"capacity");ir->foreignOutputViewCapacity=capacity;
 ir->foreignOutputViews=NULL;refused(ir,"missing rows");ir->foreignOutputViews=rows;
 ir->foreignOutputViewCount=0;refused(ir,"lost rows");ir->foreignOutputViewCount=count;
 XL6_ForeignOutputViewContract first=rows[0],second=rows[1];
 rows[0].declaration=0;refused(ir,"declaration");rows[0]=first;
 rows[0].physicalIndex=63;refused(ir,"physical output");rows[0]=first;
 rows[0].ordinal=63;refused(ir,"ordinal");rows[0]=first;
 rows[0].logicalType=0;refused(ir,"logical type");rows[0]=first;
 rows[0].conversion=XL6_FOREIGN_OUTPUT_CONVERSION_INVALID;refused(ir,"missing conversion");rows[0]=first;
 rows[0].conversion=(XL6_ForeignOutputConversion)99;refused(ir,"unknown conversion");rows[0]=first;
 rows[0].conversion=XL6_FOREIGN_OUTPUT_CONVERSION_BYTES;refused(ir,"wrong physical view");rows[0]=first;
 rows[1]=rows[0];refused(ir,"duplicate slot");rows[1]=second;
 XL6_IrNode* declaration=&ir->nodes[first.declaration-1];uint8 saved=declaration->foreignCall.valueOutput;
 declaration->foreignCall.valueOutput&=(uint8)~XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;
 refused(ir,"lost presence");declaration->foreignCall.valueOutput=saved;
 declaration->foreignCall.valueOutput|=8u;refused(ir,"unknown flags");declaration->foreignCall.valueOutput=saved;
 declaration->foreignCall.valueOutput|=32u;refused(ir,"unknown high flag");declaration->foreignCall.valueOutput=saved;
 declaration->foreignCall.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;
 refused(ir,"presence without output kind");declaration->foreignCall.valueOutput=saved;
 XL6_ForeignOutputSizeContract overlap={first.declaration,first.physicalIndex,first.ordinal,0,0,0,"size"};
 XL6_ForeignOutputSizeContract* sizes=ir->foreignOutputSizes;size_t sizeCount=ir->foreignOutputSizeCount,sizeCapacity=ir->foreignOutputSizeCapacity;
 ir->foreignOutputSizes=&overlap;ir->foreignOutputSizeCount=ir->foreignOutputSizeCapacity=1;
 refused(ir,"size/view overlap");ir->foreignOutputSizes=sizes;ir->foreignOutputSizeCount=sizeCount;ir->foreignOutputSizeCapacity=sizeCapacity;
 for(size_t i=0;i<count;++i)if(rows[i].physicalIndex==2){
  XL6_ForeignOutputViewContract row=rows[i];rows[i].physicalIndex=1;
  refused(ir,"split pointer count input");rows[i]=row;break;
 }
 unsigned targets=0;
 for(size_t t=0;t<ir->callTargetCount;++t){
  XL6_IrCallTarget* target=&ir->callTargets[t];if(!xl6IrForeignOutputViewGet(ir,target,0))continue;
  XL6_IrTypeId type=target->returnType;target->returnType=0;refused(ir,"target logical result");target->returnType=type;++targets;
 }
 assert(targets>=4 && xl6IrVerifyModule(ir,NULL));
 printf("output-view: %u targets, %u mutations refused before C/header output\n",targets,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
