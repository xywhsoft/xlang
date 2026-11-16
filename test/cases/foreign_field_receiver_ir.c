#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* reason)
{
 if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted receiver mutation: %s\n",reason);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_RECEIVER",NULL):xl6IrCgenEmitC(&out,module,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
 assert(m->foreignFieldCallCount==12);
 XL6_ForeignFieldCallContract* row=&m->foreignFieldCalls[0],saved=*row;
 row->receiverField[0]=0;refused(m,"orphan physical receiver type");*row=saved;
 row->receiverCName[0]=0;refused(m,"missing physical receiver type");*row=saved;
 memset(row->receiverField,'x',sizeof(row->receiverField));refused(m,"unterminated receiver field");*row=saved;
 memset(row->receiverCName,'x',sizeof(row->receiverCName));refused(m,"unterminated receiver type");*row=saved;
 strcpy(row->receiverField,"pUser->bad");refused(m,"field expression injection");*row=saved;
 strcpy(row->receiverCName,"int");refused(m,"non-pointer physical receiver");*row=saved;
 strcpy(row->receiverCName,"void*);bad(");refused(m,"C type injection");*row=saved;
 XL6_ForeignFieldCallContract* rows=m->foreignFieldCalls;m->foreignFieldCalls=NULL;
 refused(m,"missing receiver contract storage");m->foreignFieldCalls=rows;
 for(size_t i=0;i<m->callTargetCount;++i){
  XL6_IrCallTarget* t=&m->callTargets[i];if(t->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_FIELD_CALL)continue;
  XL6_IrCallTarget old=*t;
  t->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_CALL;refused(m,"global substitution");*t=old;
  t->parameterCount=0;refused(m,"missing logical receiver");*t=old;
  t->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m,"missing callee admission error");*t=old;
 }
 printf("FIELD_RECEIVER_IR_OK %u mutations\n",mutations);xl6CompileUnitFree(&unit);return 0;
}
