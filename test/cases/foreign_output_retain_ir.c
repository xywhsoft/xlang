#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid retained-output IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_RETAINED_OUTPUT",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);xrtFree(text);xl6OutputDiscard(&out);}
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));size_t count=ir->foreignOutputViewCount,capacity=ir->foreignOutputViewCapacity;
 XL6_ForeignOutputViewContract* rows=ir->foreignOutputViews;assert(count>=9);
 ir->foreignOutputViewCount=capacity+1;refused(ir,"count/capacity");ir->foreignOutputViewCount=count;
 ir->foreignOutputViewCapacity=count-1;refused(ir,"capacity");ir->foreignOutputViewCapacity=capacity;
 ir->foreignOutputViews=NULL;refused(ir,"lost rows");ir->foreignOutputViews=rows;
 ir->foreignOutputViewCount=0;refused(ir,"lost presence rows");ir->foreignOutputViewCount=count;
 unsigned retained=0;
 for(size_t i=0;i<count;++i){XL6_ForeignOutputViewContract* row=&rows[i];if(row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)continue;
  ++retained;XL6_ForeignOutputViewContract saved=*row;
  row->physicalIndex=63;refused(ir,"physical");*row=saved;
  row->ordinal=63;refused(ir,"ordinal");*row=saved;
  row->logicalType=0;refused(ir,"type");*row=saved;
  row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_TEXT;refused(ir,"physical text substitution");*row=saved;
  row->conversion=(XL6_ForeignOutputConversion)99;refused(ir,"conversion");*row=saved;
  row->retainSymbol=0;refused(ir,"missing retain");*row=saved;
  row->rollbackSymbol=0;refused(ir,"missing rollback");*row=saved;
  row->retainSymbol=saved.rollbackSymbol;refused(ir,"retain type");*row=saved;
  row->rollbackSymbol=saved.retainSymbol;refused(ir,"rollback type");*row=saved;
  row->retainSuccess=(XL6_ForeignRetainSuccess)0;refused(ir,"invalid success");*row=saved;
  XL6_IrNode* decl=&ir->nodes[saved.declaration-1];uint8 mode=decl->foreignCall.valueOutput;
  decl->foreignCall.valueOutput&=(uint8)~XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;refused(ir,"lost presence");decl->foreignCall.valueOutput=mode;
  uint64 flags=decl->flags;decl->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(ir,"unsafe effect");decl->flags=flags;
  if(i+1<count){XL6_ForeignOutputViewContract next=rows[i+1];rows[i+1]=saved;refused(ir,"duplicate");rows[i+1]=next;}
 }
 for(size_t t=0;t<ir->callTargetCount;++t){XL6_IrCallTarget* target=&ir->callTargets[t];
  if(!target->cName || strcmp(target->cName,"output_retain_values"))continue;
  XL6_IrTypeId result=target->returnType;target->returnType=0;refused(ir,"target result");target->returnType=result;
  XL6_IrCallResultConversionAbi conversion=target->resultConversionAbi;target->resultConversionAbi=XL6_IR_CALL_RESULT_CONVERSION_DIRECT;refused(ir,"conversion contract");target->resultConversionAbi=conversion;
 }
 /* Symbols can each be valid typed helpers but a second logical declaration
  * of the same native target must not silently choose another retain rule. */
 for(size_t i=0;i<count;++i){XL6_ForeignOutputViewContract* row=&rows[i];
  if(row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE || row->retainSuccess!=XL6_FOREIGN_RETAIN_ZERO)continue;
  XL6_IrNode* decl=&ir->nodes[row->declaration-1];if(decl->sourceKind!=XL6_AST_EXTERN_FUNC)continue;
  XL6_ForeignRetainSuccess saved=row->retainSuccess;row->retainSuccess=XL6_FOREIGN_RETAIN_POSITIVE;refused(ir,"same-C retained output contract");row->retainSuccess=saved;break;
 }
 assert(retained>=6 && xl6IrVerifyModule(ir,NULL));printf("retained outputs: %u rows, %u mutations refused before C/header output\n",retained,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
