#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
 if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid domain IR: %s\n",label);assert(0);}
 for(unsigned header=0;header<2;++header){
  XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
  assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_NATIVE_DOMAIN",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
  char* data=xl6OutputTakeMemoryText(&out,&size);assert(data && !size);xrtFree(data);xl6OutputDiscard(&out);
 }
 ++mutations;
}
int main(int argc,char** argv)
{
 assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
 assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
 XL6_IrType *root=NULL,*optional=NULL;
 for(size_t i=0;i<ir->types.count;++i){XL6_IrType* t=&ir->types.items[i];if(!t->foreignDomain)continue;if(t->kind==XL6_IR_TYPE_NULLABLE)optional=t;else if(t->kind==XL6_IR_TYPE_CLASS)root=t;}
 assert(root && optional && !strcmp(root->foreignDomain,"bindingNativeDomain"));
 char* domain=root->foreignDomain;char* nullableDomain=optional->foreignDomain;char* policy=root->foreignPolicy;
 root->foreignDomain="";refused(ir,"empty");root->foreignDomain=domain;
 root->foreignDomain="bindingNativeDomain;injection";refused(ir,"injection");root->foreignDomain=domain;
 root->foreignDomain=NULL;refused(ir,"nullable carries lost base domain");root->foreignDomain=domain;
 optional->foreignDomain=NULL;refused(ir,"nullable loses base domain");optional->foreignDomain=nullableDomain;
 optional->foreignDomain="anotherDomain";refused(ir,"nullable mismatched domain");optional->foreignDomain=nullableDomain;
 root->foreignPolicy=NULL;refused(ir,"missing policy");root->foreignPolicy=policy;
 unsigned char lifetime=root->foreignLifetime;
 root->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(ir,"caller domain");root->foreignLifetime=lifetime;
 root->foreignLifetime=XL6_FOREIGN_LIFETIME_OWNER;refused(ir,"child domain");root->foreignLifetime=lifetime;
 root->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLBACK;refused(ir,"callback domain");root->foreignLifetime=lifetime;
 assert(xl6IrVerifyModule(ir,NULL));printf("native domain: %u invalid mutations refused before C/header output\n",mutations);
 xl6CompileUnitFree(&unit);return 0;
}
