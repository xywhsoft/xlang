#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid native-header IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_FUNCTION_HEADER",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* data=xl6OutputTakeMemoryText(&out,&size);assert(data && !size);xrtFree(data);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 size_t count=ir->foreignFunctionHeaderCount,capacity=ir->foreignFunctionHeaderCapacity;assert(count>=2);
 XL6_ForeignFunctionHeaderContract* rows=ir->foreignFunctionHeaders;
 ir->foreignFunctionHeaderCount=capacity+1;refused(ir,"count beyond capacity");ir->foreignFunctionHeaderCount=count;
 ir->foreignFunctionHeaderCapacity=count-1;refused(ir,"capacity");ir->foreignFunctionHeaderCapacity=capacity;
 ir->foreignFunctionHeaders=NULL;refused(ir,"missing storage");ir->foreignFunctionHeaders=rows;
 ir->foreignFunctionHeaderCount=0;refused(ir,"missing contracts");ir->foreignFunctionHeaderCount=count;
 XL6_ForeignFunctionHeaderContract first=rows[0],second=rows[1];
 rows[0].declaration=0;refused(ir,"declaration");rows[0]=first;
 rows[0].header[0]=0;refused(ir,"empty path");rows[0]=first;
 strcpy(rows[0].header,"native.h>\n#error unsafe");refused(ir,"path injection");rows[0]=first;
 memset(rows[0].header,'x',sizeof(rows[0].header));refused(ir,"unterminated path");rows[0]=first;
 rows[1]=rows[0];refused(ir,"duplicate");rows[1]=second;
 unsigned targets=0;
 for(size_t i=0;i<ir->callTargetCount;++i){
  XL6_IrCallTarget* target=&ir->callTargets[i];uint32 saved=target->foreignHeaderIndexPlusOne;if(!saved)continue;++targets;
  target->foreignHeaderIndexPlusOne=0;refused(ir,"lost target contract");target->foreignHeaderIndexPlusOne=saved;
  target->foreignHeaderIndexPlusOne=(uint32)count+1;refused(ir,"target range");target->foreignHeaderIndexPlusOne=saved;
  target->foreignHeaderIndexPlusOne=saved==1 ? 2:1;refused(ir,"target references another declaration");target->foreignHeaderIndexPlusOne=saved;
 }
 assert(targets>=2 && xl6IrVerifyModule(ir,NULL));printf("native function headers: %u targets, %u mutations refused before C/header output\n",targets,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
