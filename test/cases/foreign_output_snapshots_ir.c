#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned snapshotMutations;
static void snapshotRefused(XL6_IrModule* m,const char* name)
{
 if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"unexpected valid output snapshot IR: %s\n",name);assert(0);}
 for(int header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header?xl6IrCgenEmitHeader(&out,m,"BAD_SNAPSHOT_OUTPUT",NULL):xl6IrCgenEmitC(&out,m,NULL)));
  char* bytes=xl6OutputTakeMemoryText(&out,&size);assert(bytes&&!size);xrtFree(bytes);xl6OutputDiscard(&out);}
 ++snapshotMutations;
}
int main(int argc,char** argv)
{
 assert(argc==2&&sizeof(XL6_ForeignCallFacts)==88&&sizeof(XL6_ForeignOutputViewContract)==32);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));unsigned owned=0,strings=0;
 for(size_t i=0;i<m->foreignOutputViewCount;++i){XL6_ForeignOutputViewContract* row=&m->foreignOutputViews[i],saved=*row;
  if(row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING&&row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE)continue;
  row->physicalIndex=63;snapshotRefused(m,"physical output");*row=saved;
  row->ordinal=63;snapshotRefused(m,"logical ordinal");*row=saved;
  row->logicalType=0;snapshotRefused(m,"lost logical type");*row=saved;
  row->retainSymbol=1;snapshotRefused(m,"unexpected retain");*row=saved;
  row->rollbackSymbol=1;snapshotRefused(m,"unexpected rollback");*row=saved;
  row->retainSuccess=1;snapshotRefused(m,"unexpected retain success");*row=saved;
  XL6_IrNode* node=&m->nodes[row->declaration-1];uint64 flags=node->flags;
  node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;snapshotRefused(m,"missing native error boundary");node->flags=flags;
  if(row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE){++owned;
   node->flags|=XL6_AST_FLAG_FOREIGN_PURE;snapshotRefused(m,"pure owning producer");node->flags=flags;
   row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING;snapshotRefused(m,"value becomes text");*row=saved;
  }else{++strings;row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE;snapshotRefused(m,"text becomes owning value");*row=saved;}
 }
 assert(owned==2&&strings>=6);
 for(size_t i=0;i<m->foreignOutputFieldCount;++i){XL6_ForeignOutputFieldContract* row=&m->foreignOutputFields[i],saved=*row;
  row->logicalType=0;snapshotRefused(m,"field result type");*row=saved;
  strcpy(row->field,"first[0]");snapshotRefused(m,"field expression");*row=saved;
 }
 assert(xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&unit);
 printf("Output snapshot IR: %u mutations rejected before any C/header bytes\n",snapshotMutations);return 0;
}
