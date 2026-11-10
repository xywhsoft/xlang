/* Logical policy aliases never relax sealed operands or physical contracts. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* module,const char* label)
{
 if(xl6IrVerifyModule(module,NULL)){fprintf(stderr,"accepted damaged policy input: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_POLICY_INPUT",NULL):xl6IrCgenEmitC(&out,module,NULL)));
  char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
 assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
 XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
 XL6_IrTypeId first=0,second=0;unsigned targets=0;
 for(size_t i=0;i<module->callTargetCount;++i){
  XL6_IrCallTarget* target=&module->callTargets[i];XL6_IrNode* node=&module->nodes[target->declaration-1];
  if(node->sourceKind!=XL6_AST_EXTERN_FUNC || target->parameterCount!=1 || !target->cName ||
    (strcmp(target->cName,"policyInputRelease") && strcmp(target->cName,"policyInputKind")))continue;
  ++targets;
  if(!first)first=target->parameterTypes[0];else if(target->parameterTypes[0]!=first)second=target->parameterTypes[0];
  XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[0];
  target->parameterPhysicalAbis[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,"raw physical input");target->parameterPhysicalAbis[0]=physical;
  if(!strcmp(target->cName,"policyInputRelease")){
   int32 code=node->foreignCall.successCode,callee=target->nativeSuccessCode;
   node->foreignCall.successCode=target->nativeSuccessCode=1;refused(module,"coherent but conflicting success status");
   node->foreignCall.successCode=code;target->nativeSuccessCode=callee;
  }
 }
 assert(targets>=4 && first && second);
 XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&module->types,second);assert(type && type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED);
 char* domain=type->foreignDomain;type->foreignDomain="policyInputOtherDomain";refused(module,"different admission domain");type->foreignDomain=domain;
 char cname[sizeof(type->cName)];memcpy(cname,type->cName,sizeof(cname));strcpy(type->cName,"PolicyOther*");refused(module,"different native pointer");memcpy(type->cName,cname,sizeof(cname));
 for(size_t i=0;i<module->callTargetCount;++i){
  XL6_IrCallTarget* target=&module->callTargets[i];XL6_IrNode* node=&module->nodes[target->declaration-1];
  if(node->sourceKind!=XL6_AST_EXTERN_FUNC || !target->cName || strcmp(target->cName,"policyInputKind") || target->parameterCount!=1)continue;
  XL6_IrTypeId saved=target->parameterTypes[0];target->parameterTypes[0]=saved==first ? second:first;
  refused(module,"nominal operand substitution");target->parameterTypes[0]=saved;
 }
 assert(mutations>=10 && xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
 printf("Policy input aliases: %u damaged IR contracts refused before C/header output\n",mutations);return 0;
}
