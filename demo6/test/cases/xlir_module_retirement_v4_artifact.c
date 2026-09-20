/* Deliberately explicit ABI fixture, compiled/relocated by real TCC. Generated
 * language programs are exercised separately; this probes the host contract. */
#include <__xl_runtime/xruntime_decl.h>
#include <__xl_runtime/xl6_code_lease.h>
#include "xlir_module_retirement_v4_hooks.h"
#ifndef CONTRACT_BAD
#define CONTRACT_BAD 0
#endif
static RetirementHooks* hooks;
static XL6_CodeOwnerGraph owner;
static XL6_CodeLease* lease;
static int initialized, done;
void retirement_bind_hooks(RetirementHooks* value) { hooks=value; }
int xl_script_bind_code_owner(const XL6_CodeOwner* value)
{
    if(!value || value->size!=sizeof(owner))return 0;
    owner=*(const XL6_CodeOwnerGraph*)value;return 1;
}
int xl_script_module_init(xvalue* arg)
{
    (void)arg;
    if(initialized)return 1;
    ++hooks->initializations;done=0;initialized=1;
    if(hooks->ownLease && xl6CodeLeaseAcquire(&owner.base,&lease)!=XL6_CODE_LEASE_ACQUIRED)return 0;
    return !hooks->failInit;
}
int xl_script_main_checked(xvalue* arg,int* result)
{ *result=42;return xl_script_module_init(arg); }
bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit visit,void* data)
{ return visit && visit(xl6CodeLeaseOwnership(lease),data); }
static XL6_ModulePrepareResult prepare(void) { return XL6_MODULE_PREPARE_READY; }
static XL6_ModuleRetireResult step(void)
{
    ++hooks->steps;
    if(initialized){
        ++hooks->finalizers;initialized=0;
        XL6_CodeLease* retired=lease;lease=NULL;xl6CodeLeaseRelease(retired);
    }
    if(hooks->status==XL6_MODULE_RETIRE_READY && !done){++hooks->completions;done=1;}
    return (XL6_ModuleRetireResult)hooks->status;
}
int xl_script_module_unit_checked(void) { ++hooks->legacyCalls;return 0; }
static const XL6_ModuleRetirementV1 graph={sizeof(graph),0x584C364D525431ULL,
    xl_script_module_ownership_trace,xl_script_module_unit_checked};
const XL6_ModuleRetirementV1* xl_script_module_retirement_v1(void) { return &graph; }
const XL6_ModuleRetirementV4* xl_script_module_retirement_v4(void)
{
    static const XL6_ModuleRetirementV4 api={CONTRACT_BAD==2?0:sizeof(api),
        CONTRACT_BAD==3?0:0x584C364D525434ULL,CONTRACT_BAD==4?NULL:&graph,
        CONTRACT_BAD==5?NULL:prepare,CONTRACT_BAD==6?NULL:step};
    return CONTRACT_BAD==1?NULL:&api;
}
#if CONTRACT_BAD==7
const XL6_ModuleRetirementV3* xl_script_module_retirement_v3(void)
{
    static const XL6_ModuleRetirementV3 api={sizeof(api),0x584C364D525433ULL,&graph,prepare};return &api;
}
#endif
