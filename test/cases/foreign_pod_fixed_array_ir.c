#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static void refused(XL6_IrModule* m)
{
 assert(!xl6IrVerifyModule(m,NULL));
 for(unsigned h=0;h<2;++h){XL6_Output out;size_t n=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(h ? xl6IrCgenEmitHeader(&out,m,"BAD_POD_ARRAY",NULL):xl6IrCgenEmitC(&out,m,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&n);assert(text && !n);xrtFree(text);xl6OutputDiscard(&out);
 }
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit u;xl6CompileUnitInit(&u);
 assert(xl6CompileUnitLoadFile(&u,argv[1],1) && xl6IrLowerModule(&u.ir,&u.parse.diags));
 XL6_IrModule* m=&u.ir;assert(xl6IrVerifyModule(m,NULL));unsigned changes=0;
 for(size_t i=0;i<m->types.count;++i){XL6_IrType* t=&m->types.items[i];if(t->kind!=XL6_IR_TYPE_C_ARRAY)continue;
  uint64 extent=t->nativeArrayExtent;t->nativeArrayExtent=0;refused(m);t->nativeArrayExtent=extent;++changes;
  XL6_IrOwnershipAbi abi=t->ownershipAbi;t->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refused(m);t->ownershipAbi=abi;++changes;
 }
 assert(changes>=4 && xl6IrVerifyModule(m,NULL));xl6CompileUnitFree(&u);
 printf("embedded fixed C array POD: %u sealed mutations rejected before output\n",changes);return 0;
}
