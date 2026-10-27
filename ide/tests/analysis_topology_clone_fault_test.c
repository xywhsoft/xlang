#include "analysis/input_topology.h"
#include "src/compile/xl6_source_overlay.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
/* Linker interception covers every allocation/container operation in the
 * actual topology copier;XRT container internals retain their own contracts. */
extern void* __real_xrtMalloc(size_t);
extern void* __real_xrtCalloc(size_t,size_t);
extern void __real_xrtFree(void*);
extern bool __real_xrtArrayInit(xarray*,size_t);
extern bool __real_xrtArrayReserve(xarray*,size_t);
extern bool __real_xrtArrayPush(xarray*,const void*);
extern void __real_xrtArrayUnit(xarray*);
static int enabled,trackingError;static size_t calls,deny,live,arrayLive;
static void* pointers[4096];static xarray* arrays[64];
static int refuse(void){if(!enabled)return 0;++calls;return deny && calls==deny;}
static void remember(void* p){if(p && enabled){for(size_t i=0;i<4096;++i)if(!pointers[i]){pointers[i]=p;++live;return;}trackingError=1;}}
static void forget(void* p){if(p)for(size_t i=0;i<4096;++i)if(pointers[i]==p){pointers[i]=NULL;--live;return;}}
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p);return p;}
void* __wrap_xrtCalloc(size_t n,size_t size){if(refuse())return NULL;void* p=__real_xrtCalloc(n,size);remember(p);return p;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
bool __wrap_xrtArrayInit(xarray* a,size_t size)
{
    if(refuse())return false;
    bool okay=__real_xrtArrayInit(a,size);
    if(okay && enabled){for(size_t i=0;i<64;++i)if(!arrays[i]){arrays[i]=a;++arrayLive;return okay;}trackingError=1;}
    return okay;
}
bool __wrap_xrtArrayReserve(xarray* a,size_t n){return !refuse() && __real_xrtArrayReserve(a,n);}
bool __wrap_xrtArrayPush(xarray* a,const void* p){return !refuse() && __real_xrtArrayPush(a,p);}
void __wrap_xrtArrayUnit(xarray* a){for(size_t i=0;i<64;++i)if(arrays[i]==a){arrays[i]=NULL;--arrayLive;break;}__real_xrtArrayUnit(a);}
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_topology_clone_fault_test:%d: %s calls=%zu deny=%zu live=%zu arrays=%zu\n",__LINE__,#v,calls,deny,live,arrayLive);return 0;}}while(0)
typedef struct stop_at {unsigned calls,at;} stop_at;
static int cancelled(void* user){stop_at* stop=user;return ++stop->calls>=stop->at;}
static int run(void)
{
    char *a=xrtPathAbs("topology-clone-A.xl"),*b=xrtPathAbs("topology-clone-中文😀.xl"),*other=xrtPathAbs("topology-clone-absent.xl");CHECK(a && b && other);
    XL6_SourceOverlay overlay={0};CHECK(xl6SourceOverlaySet(&overlay,a,"",0));xls_input_topology_scope scope;xlsInputTopologyBegin(&scope,8u*1024u*1024u);xlsInputTopologyBind(&overlay,1);xlsInputTopologyLookup(&overlay,a);xlsInputTopologyLookup(&overlay,b);xls_input_topology* original=xlsInputTopologyEnd(&scope);CHECK(original && xlsInputTopologyInfo(original).paths==2);
    enabled=1;calls=deny=0;xls_input_topology* copy=xlsInputTopologyClone(original,8u*1024u*1024u,NULL,NULL);size_t operations=calls;
    CHECK(copy && operations>=7 && xlsInputTopologyInfo(copy).paths==2);size_t exact=xlsInputTopologyInfo(copy).bytes;xlsInputTopologyDestroy(copy);CHECK(!live && !arrayLive && !trackingError);
    for(size_t i=1;i<=operations;++i){calls=0;deny=i;copy=xlsInputTopologyClone(original,8u*1024u*1024u,NULL,NULL);CHECK(!copy && calls>=i && !live && !arrayLive && !trackingError);CHECK(xlsInputTopologyInfo(original).paths==2);}
    deny=0;
    for(unsigned i=0;i<3;++i){copy=xlsInputTopologyClone(original,i==0 ? 0 : i==1 ? exact-1 : exact,NULL,NULL);CHECK((copy!=NULL)==(i==2));xlsInputTopologyDestroy(copy);CHECK(!live && !arrayLive);}
    stop_at probe={0,~0u};copy=xlsInputTopologyClone(original,8u*1024u*1024u,cancelled,&probe);CHECK(copy && probe.calls);xlsInputTopologyDestroy(copy);unsigned points=probe.calls;
    for(unsigned i=1;i<=points;++i){stop_at stop={0,i};copy=xlsInputTopologyClone(original,8u*1024u*1024u,cancelled,&stop);CHECK(!copy && stop.calls>=i && !live && !arrayLive);}
    copy=xlsInputTopologyClone(original,8u*1024u*1024u,NULL,NULL);CHECK(copy);enabled=0;
    xlsInputTopologyDestroy(original);xl6SourceOverlayClear(&overlay);
    const char* changed[]={other};const char* current[]={a,other};CHECK(xlsInputTopologyAdmits(copy,changed,1,current,2));changed[0]=a;CHECK(!xlsInputTopologyAdmits(copy,changed,1,current,2));
    xlsInputTopologyDestroy(copy);CHECK(!live && !arrayLive && !trackingError);xrtFree(a);xrtFree(b);xrtFree(other);
    printf("topology clone refused all%zu operation boundaries and%u cancellation positions\n",operations,points);return 1;
}
int main(void){if(!run())return 1;printf("analysis_topology_clone_fault_test passed: %u checks\n",checks);return 0;}
