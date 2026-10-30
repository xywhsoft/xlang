/* Compiled by check_native_bridge_catalog.py with independently named,
 * unmodified reference lookup/policy bodies and the real current producers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t checks;
static int query(const char* name)
{
    ++checks;
    const char* a=oldPolicy(name),*b=xl6IrRuntimeNativeHandlePolicyForName(name);
    if(a && b ? !strcmp(a,b) : a==b)return 1;
    fprintf(stderr,"policy %s: reference=%s,current=%s\n",name ? name : "(null)",a ? a : "(null)",b ? b : "(null)");
    return 0;
}
static int variants(const char* name)
{
    size_t length=strlen(name);char* text=malloc(length+4);
    if(!text)return 0;
    if(!query(name)){free(text);return 0;}
    const OldContract* a=oldLookup(name);
    const XL6_IrDirectBridgeContract* b=xl6IrDirectBridgeContract(name);
    ++checks;
    if((a!=NULL)!=(b!=NULL) || (a && (strcmp(a->name,b->name) || a->parameterCount!=b->parameterCount))){free(text);return 0;}
    text[0]='"';memcpy(text+1,name,length);text[length+1]='"';text[length+2]=0;
    if(!query(text)){free(text);return 0;}
    a=oldSpelling(text);b=xl6IrDirectBridgeContractSpelling(text);++checks;
    if((a!=NULL)!=(b!=NULL) || (a && (strcmp(a->name,b->name) || a->parameterCount!=b->parameterCount))){free(text);return 0;}
    text[length+1]=0;
    if(!query(text)){free(text);return 0;}
    memcpy(text,name,length);text[length]=' ';text[length+1]=0;
    if(!query(text)){free(text);return 0;}
    free(text);return 1;
}
int main(void)
{
    if(!query(NULL) || !query("") || !variants("not_a_contract") || !variants("\xff"))return 1;
    for(size_t i=0;i<sizeof(xl6IrDirectBridgeContracts)/sizeof(xl6IrDirectBridgeContracts[0]);++i)
        if(!variants(xl6IrDirectBridgeContracts[i].name))return 1;
    static const char* resources[]={
#define XL6_NATIVE_RESOURCE_POLICY(Owner,Component,Getter) Owner,
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
    };
    for(size_t i=0;i<sizeof(resources)/sizeof(resources[0]);++i)
        if(resources[i] ? !variants(resources[i]) : !query(NULL))return 1;
    printf("analysis_bridge_catalog_test passed: %zu checks; canonical/quoted/unknown lookup and original resource-policy fallback\n",checks);
    return 0;
}
