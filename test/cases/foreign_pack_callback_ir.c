#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"accepted packed callback damage %u: %s\n",mutations,label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_PACKED_CALLBACK",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 assert(sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignPackFieldContract)==220);
 unsigned callbackFields=0,userFields=0,owners=0,nested=0,voidCallbacks=0;
 for(size_t i=0;i<ir->foreignPackFieldCount;++i){
  XL6_ForeignPackFieldContract* field=&ir->foreignPackFields[i];
  if(field->kind!=XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK && field->kind!=XL6_FOREIGN_PACK_FIELD_CALLBACK_USER)continue;
  XL6_ForeignPackFieldContract saved=*field;
  if(field->kind==XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK)++callbackFields;else ++userFields;
  if(field->recordParentIndexPlusOne)++nested;
  field->kind=XL6_FOREIGN_PACK_FIELD_INVALID;refused(ir,"missing field kind");*field=saved;
  field->kind=(XL6_ForeignPackFieldKind)99;refused(ir,"unknown field kind");*field=saved;
  field->kind=saved.kind==XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK ? XL6_FOREIGN_PACK_FIELD_CALLBACK_USER:XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK;
  refused(ir,"unpaired callback/user");*field=saved;
  field->parameter=63;refused(ir,"source outside descriptor");*field=saved;
  field->logicalType=0;refused(ir,"lost callable type");*field=saved;
  field->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(ir,"spurious span ABI");*field=saved;
  field->recordParentIndexPlusOne^=1;refused(ir,"wrong containing record");*field=saved;
  strcpy(field->physicalCName,saved.kind==XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK ? "void*":"const void*");
  refused(ir,"wrong exact C field type");*field=saved;
  memset(field->physicalCName,'x',sizeof(field->physicalCName));refused(ir,"unterminated field type");*field=saved;
  memset(field->field,'x',sizeof(field->field));refused(ir,"unterminated field path");*field=saved;
  if(saved.kind!=XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK)continue;
  XL6_IrNode* node=&ir->nodes[field->declaration-1];
  XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(ir,node->symbol);assert(target && field->parameter<target->parameterCount);
  XL6_ForeignParameterContract* callback=(XL6_ForeignParameterContract*)xl6IrForeignParameterGet(ir,target,field->parameter);assert(callback);
  XL6_ForeignParameterContract original=*callback;
  if(callback->callbackOwnerIndexPlusOne)++owners;
  callback->kind=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK;refused(ir,"escaping callback in transient descriptor");*callback=original;
  callback->kind=XL6_FOREIGN_PARAMETER_SPAN;refused(ir,"callback changed to span");*callback=original;
  callback->callbackTypeCName[0]='!';refused(ir,"invalid callback typedef");*callback=original;
  memset(callback->callbackTypeCName,'x',sizeof(callback->callbackTypeCName));refused(ir,"unterminated callback typedef");*callback=original;
  callback->callbackOwnerIndexPlusOne=65;refused(ir,"unknown owner");*callback=original;
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[field->parameter];
  target->parameterPhysicalAbis[field->parameter]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"lost scoped callback preparation");
  target->parameterPhysicalAbis[field->parameter]=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK;refused(ir,"retained callback preparation");
  target->parameterPhysicalAbis[field->parameter]=physical;
  uint32 flags=node->flags;node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(ir,"pure callable entry");node->flags=flags;
  node->flags|=XL6_AST_FLAG_FOREIGN_NONBLOCKING;refused(ir,"nonblocking scoped callback");node->flags=flags;
  const XL6_IrType* signature=xl6IrForeignCallbackSignature(ir,target,field->parameter);assert(signature);
  if(!signature->returnCount){++voidCallbacks;callback->callbackFailureCode=1;refused(ir,"void callback with failure value");*callback=original;}
 }
 assert(callbackFields>=10 && callbackFields==userFields && owners && nested && voidCallbacks);
 assert(xl6IrVerifyModule(ir,NULL));
 printf("Packed callback IR: %u callback/user pairs, %u mutations rejected before C/header output; facts=88 field-row=220\n",callbackFields,mutations);
 xl6CompileUnitFree(&unit);return 0;
}
