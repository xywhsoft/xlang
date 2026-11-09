#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* name)
{
 if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted damaged projection IR: %s\n",name);assert(0);}
 for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_PROJECTION",NULL):xl6IrCgenEmitC(&out,module,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
 assert(m->foreignPackCount>=4 && m->foreignArrayGroupCount);
 for(size_t i=0;i<m->foreignPackCount;++i){XL6_ForeignPackContract* row=&m->foreignPacks[i],saved=*row;
  strcpy(row->recordCName,"ProjectionOtherDescriptor");refused(m,"pack C pointer type");*row=saved;
 }
 for(size_t i=0;i<m->foreignArrayInputCount;++i){XL6_ForeignArrayInputContract* row=&m->foreignArrayInputs[i],saved=*row;
  strcpy(row->pointerCName,"const int*");refused(m,"array physical pointer");*row=saved;
  row->kind=XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW;refused(m,"array lifetime");*row=saved;
 }
 for(size_t i=0;i<m->foreignArrayGroupCount;++i){XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i],saved=*row;
  row->countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;refused(m,"array count physical ABI");*row=saved;
  ++row->physicalIndex;refused(m,"array pointer physical slot");*row=saved;
 }
 for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* target=&m->callTargets[i];
  if(!target->cName || strcmp(target->cName,"projectionStatus"))continue;
  XL6_IrNode* node=&m->nodes[target->declaration-1];int32 savedCode=node->foreignCall.successCode;
  int32 savedTarget=target->nativeSuccessCode;node->foreignCall.successCode=target->nativeSuccessCode=1;
  refused(m,"changed native success contract");node->foreignCall.successCode=savedCode;target->nativeSuccessCode=savedTarget;
 }
 assert(mutations>=7);assert(xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&unit);
 printf("Projection overload sealed IR mutations: %u\n",mutations);return 0;
}
