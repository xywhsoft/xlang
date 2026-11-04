#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* reason)
{
 if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted callable-field mutation: %s\n",reason);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_CALLABLE_FIELD",NULL):xl6IrCgenEmitC(&out,module,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);
 XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
 assert(m->foreignFieldCallCount==12);
 size_t count=m->foreignFieldCallCount,capacity=m->foreignFieldCallCapacity;
 XL6_ForeignFieldCallContract* rows=m->foreignFieldCalls;
 m->foreignFieldCalls=NULL;refused(m,"missing sparse storage");m->foreignFieldCalls=rows;
 m->foreignFieldCallCount=capacity+1;refused(m,"count exceeds arena");m->foreignFieldCallCount=count;
 m->foreignFieldCallCount=0;refused(m,"lost field contracts");m->foreignFieldCallCount=count;
 XL6_ForeignFieldCallContract saved=rows[0];
 rows[0].declaration=0;refused(m,"missing declaration");rows[0]=saved;
 rows[0].field[0]=0;refused(m,"empty field");rows[0]=saved;
 memset(rows[0].field,'x',sizeof(rows[0].field));refused(m,"unterminated field");rows[0]=saved;
 strcpy(rows[0].field,"count");refused(m,"field identity replaced");rows[0]=saved;
 XL6_ForeignFieldCallContract* duplicate=xrtCalloc(count+1,sizeof(*duplicate));assert(duplicate);
 memcpy(duplicate,rows,count*sizeof(*rows));duplicate[count]=rows[0];
 m->foreignFieldCalls=duplicate;m->foreignFieldCallCount=m->foreignFieldCallCapacity=count+1;
 refused(m,"duplicate field contract");m->foreignFieldCalls=rows;m->foreignFieldCallCount=count;m->foreignFieldCallCapacity=capacity;xrtFree(duplicate);
 unsigned methods=0;
 for(size_t i=0;i<m->callTargetCount;++i){
  XL6_IrCallTarget* t=&m->callTargets[i];if(t->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_FIELD_CALL)continue;
  ++methods;XL6_IrCallTarget target=*t;XL6_IrNode* n=&m->nodes[t->declaration-1];XL6_IrNode node=*n;
  t->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_CALL;refused(m,"global call substitution");*t=target;
  t->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_FIELD_READ;refused(m,"field read substitution");*t=target;
  t->foreignOperationAbi=XL6_IR_FOREIGN_OPERATION_INVALID;refused(m,"unknown operation");*t=target;
  t->cName=NULL;refused(m,"missing callee identity");*t=target;
  t->parameterCount=0;refused(m,"missing receiver");*t=target;
  t->variadicAbi=XL6_IR_CALL_VARIADIC_C;refused(m,"unsealed variadic ABI");*t=target;
  t->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(m,"missing callee admission error");*t=target;
  t->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m,"missing checked outcome");*t=target;
  t->preparationFailureAbi=XL6_IR_CALL_PREPARATION_NONE;refused(m,"missing preparation cleanup");*t=target;
  n->flags|=XL6_AST_FLAG_STATIC;refused(m,"static receiver");*n=node;
  n->foreignCall.receiverDereference=1;refused(m,"dereference overlap");*n=node;
  n->sourceKind=XL6_AST_EXTERN_FUNC;refused(m,"free declaration");*n=node;
  XL6_IrType* receiver=(XL6_IrType*)xl6IrTypeGet(&m->types,t->parameterTypes[0]);
  char receiverCName[sizeof(receiver->cName)];memcpy(receiverCName,receiver->cName,sizeof(receiverCName));
  strcpy(receiver->cName,"BindingFieldTable**");assert(!xl6IrForeignProjectionMatches(m,t));
  refused(m,"multi-level receiver pointer");memcpy(receiver->cName,receiverCName,sizeof(receiverCName));
  assert(xl6IrVerifyModule(m,NULL));
 }
 assert(methods==12);printf("callable fields: %u methods, %u mutations rejected with empty C/header output\n",methods,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
