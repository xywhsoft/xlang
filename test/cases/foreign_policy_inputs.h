#ifndef XL6_FIXTURE_FOREIGN_POLICY_INPUTS_H
#define XL6_FIXTURE_FOREIGN_POLICY_INPUTS_H
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
#include <stdlib.h>
typedef struct PolicyInput {int kind;} PolicyInput;
static int policyInputLive,policyInputDestroyedA,policyInputDestroyedB;
static int policyInputCreate(PolicyInput** out,int kind)
{*out=NULL;PolicyInput* value=(PolicyInput*)xrtMalloc(sizeof(*value));if(!value)return -1;value->kind=kind;++policyInputLive;*out=value;return 0;}
static int policyInputCreateA(PolicyInput** out){return policyInputCreate(out,1);}
static int policyInputCreateB(PolicyInput** out){return policyInputCreate(out,2);}
static int policyInputRelease(PolicyInput* value)
{if(!value)return 0;--policyInputLive;xrtFree(value);return 0;}
static int policyInputKind(PolicyInput* value){return value->kind;}
static bool policyInputDestroyA(void* raw)
{PolicyInput* value=(PolicyInput*)raw;if(value->kind!=1)abort();++policyInputDestroyedA;return policyInputRelease(value)==0;}
static bool policyInputDestroyB(void* raw)
{PolicyInput* value=(PolicyInput*)raw;if(value->kind!=2)abort();++policyInputDestroyedB;return policyInputRelease(value)==0;}
static const XL6_ForeignViewDescriptorV1 policyInputPolicyA={sizeof(XL6_ForeignViewDescriptorV1),XL6_FOREIGN_VIEW_V1_TAG,
 "policy.input.a",0,1,NULL,NULL,policyInputDestroyA,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static const XL6_ForeignViewDescriptorV1 policyInputPolicyB={sizeof(XL6_ForeignViewDescriptorV1),XL6_FOREIGN_VIEW_V1_TAG,
 "policy.input.b",0,1,NULL,NULL,policyInputDestroyB,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static bool policyInputBalanced(void){return !policyInputLive && policyInputDestroyedA>0 && policyInputDestroyedB>0;}
#endif
