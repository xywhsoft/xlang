#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
static uint64_t parameterMask;
static size_t parameterVisits;
static unsigned parameterOrder[64];
static void parameterBitHit(unsigned bit)
{if(parameterVisits<64)parameterOrder[parameterVisits]=bit;++parameterVisits;parameterMask|=UINT64_C(1)<<bit;}
#define XLS_IR_PARAMETER_BIT_HIT(p) parameterBitHit((unsigned)(p))
#include "xl6_ir.c"
#ifdef XLS_NATIVE_IR_PARAMETER_BITS
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ir_parameters_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int compare(XL6_IrModule* module,int expected,uint64_t mask,size_t visits)
{
 XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);parameterMask=0;parameterVisits=0;
 int actual=xl6IrVerifyForeignParameters(module,&a),reference=xlsIrParameterVerifyOriginal(module,&b);
 CHECK(actual==expected && reference==expected && a.count==b.count);
 for(size_t i=0;i<a.count;++i)CHECK(a.items[i].line==b.items[i].line && a.items[i].column==b.items[i].column && !strcmp(a.items[i].message,b.items[i].message));
 CHECK(parameterMask==mask && parameterVisits==visits);
 size_t at=0;for(unsigned bit=0;bit<64;++bit)if(mask&(UINT64_C(1)<<bit)){CHECK(at<visits && parameterOrder[at]==bit);++at;}
 xl6DiagFree(&a);xl6DiagFree(&b);return 1;
}
static int run(void)
{
 XL6_IrNode nodes[3]={0};XL6_IrNodeId children[]={3,2};XL6_ForeignParameterContract rows[65]={0};
 XL6_IrModule module={0};module.nodes=nodes;module.nodeCount=module.nodeCapacity=3;module.children=children;module.childCount=module.childCapacity=2;
 module.foreignParameters=rows;module.foreignParameterCapacity=65;
 nodes[0].kind=XL6_IR_NODE_FUNC_DECL;nodes[0].childCount=2;nodes[1].kind=XL6_IR_NODE_LIST;nodes[1].childCount=64;
 CHECK(compare(&module,1,0,0));
 module.foreignParameterCount=66;CHECK(compare(&module,0,0,0));module.foreignParameterCount=1;module.foreignParameters=NULL;CHECK(compare(&module,0,0,0));module.foreignParameterCount=0;module.foreignParameters=rows;
 /* Every bit, including bit 63, must be visited and rejected when its native
  * row is absent. Poisoning an unrelated node must not hide that contract. */
 for(unsigned bit=0;bit<64;++bit){nodes[2].foreignCall.physicalPairs=UINT64_C(1)<<bit;CHECK(compare(&module,0,UINT64_C(1)<<bit,1));}
 nodes[2].foreignCall.physicalPairs=0;
 uint64_t masks[]={0,1,UINT64_C(1)<<63,UINT64_C(0x8000000180000001),UINT64_MAX};
 for(size_t k=0;k<sizeof(masks)/sizeof(*masks);++k){
  uint64_t mask=masks[k];nodes[0].foreignCall.physicalPairs=mask;nodes[0].foreignCall.retainedCallbacks=0;size_t count=0;
  for(unsigned bit=0;bit<64;++bit)if(mask&(UINT64_C(1)<<bit)){
   XL6_ForeignParameterContract row={0};row.declaration=1;row.parameter=bit;row.kind=XL6_FOREIGN_PARAMETER_SPAN;row.countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;
   strcpy(row.pointerCName,"uint8_t*");strcpy(row.elementCName,"uint8_t");rows[count++]=row;
  }
  module.foreignParameterCount=count;CHECK(compare(&module,1,mask,count));
  if(count){
   /* Missing and duplicate rows retain the original rejection and message. */
   module.foreignParameterCount=count-1;
   CHECK(compare(&module,0,mask,count));module.foreignParameterCount=count;
   rows[count]=rows[0];module.foreignParameterCount=count+1;CHECK(compare(&module,0,0,0));module.foreignParameterCount=count;
   nodes[0].foreignCall.retainedCallbacks=mask;CHECK(compare(&module,1,mask,count));
   nodes[0].foreignCall.physicalPairs=0;
   for(size_t i=0;i<count;++i){rows[i].kind=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK;rows[i].countAbi=0;rows[i].pointerCName[0]=rows[i].elementCName[0]=0;strcpy(rows[i].callbackOwnerCName,"owner");snprintf(rows[i].callbackTypeCName,sizeof(rows[i].callbackTypeCName),"xlir_retained_d1_p%u",(unsigned)rows[i].parameter);}
   CHECK(compare(&module,1,mask,count));
  }
 }
 return 1;
}
int main(void){if(!run())return 1;printf("analysis_ir_parameters_test passed: %u checks; native original diagnostic parity, all 64 bits and sparse/dense span/retained masks\n",checks);return 0;}
#else
int main(void){puts("analysis_ir_parameters_test: original verifier retained for this cohort");return 0;}
#endif
