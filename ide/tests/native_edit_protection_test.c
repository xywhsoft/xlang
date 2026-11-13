#define _WIN32_WINNT 0x0601
#include "services/native_edit_protection.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;static size_t calls,failAt,opens;static int probing;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"native-protection:%d:%s\n",__LINE__,#v);exit(1);}}while(0)
static int refuse(void){return probing&&++calls==failAt;}
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);
void* __wrap_malloc(size_t n){return refuse()?NULL:__real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){return refuse()?NULL:__real_calloc(n,z);}
extern void*(*__real___imp_xrtCalloc)(size_t,size_t);
static void* xc(size_t n,size_t z){return refuse()?NULL:__real___imp_xrtCalloc(n,z);}
void*(*__wrap___imp_xrtCalloc)(size_t,size_t)=xc;
extern HANDLE(WINAPI*__real___imp_CreateFileW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static HANDLE WINAPI openProbe(LPCWSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES x,DWORD c,DWORD f,HANDLE t)
{if(probing)++opens;return __real___imp_CreateFileW(p,a,s,x,c,f,t);}
HANDLE(WINAPI*__wrap___imp_CreateFileW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE)=openProbe;
static int has(const xide_native_protection* p,const char* path,int directory)
{const xide_edit_alias_exclusion* rows=xideNativeProtectionPaths(p);for(size_t i=0;i<xideNativeProtectionCount(p);++i)if(rows[i].directory==directory&&xidePathLexicalEqual(rows[i].path,path))return 1;return 0;}
int main(void)
{
 const char* manifests[]={"F:/host-policy/project/一.xlp","f:/HOST-policy/project/二.xlp"};
 xide_native_protection_seed seed={"F:/host-policy/runtime","F:/host-policy/compiler/xl.exe","F:/host-policy/state",NULL,"F:/host-policy/settings.xl","F:/host-policy/workspace.xl",NULL,manifests,2};xide_model_error error={0};
 DWORD handles,after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));
 probing=1;xide_native_protection* policy=xideNativeProtectionCreate(&seed,&error);probing=0;CHECK(policy&&calls&&!opens);size_t allocations=calls;
 CHECK(has(policy,"f:/HOST-policy/runtime/extlibs",1)&&has(policy,"F:/host-policy/compiler/include",1)&&has(policy,seed.storageDirectory,1)&&has(policy,seed.preferences,0)&&has(policy,manifests[1],0));
 CHECK(!has(policy,"F:/host-policy/runtime/examples",1)&&!has(policy,"F:/host-policy/runtime/projects",1)&&!has(policy,"F:/host-policy/runtime",1)&&!has(policy,"F:/host-policy/project",1));
 const char* reversed[]={manifests[1],"f:/HOST-policy/project/一.xlp",manifests[0]};seed.manifests=reversed;seed.projects=3;xide_native_protection* same=xideNativeProtectionCreate(&seed,&error);CHECK(same&&xideNativeProtectionEqual(policy,same));xideNativeProtectionRelease(same);
 seed.preferences="F:/host-policy/other-settings.xl";same=xideNativeProtectionCreate(&seed,&error);CHECK(same&&!xideNativeProtectionEqual(policy,same));xideNativeProtectionRelease(same);seed.preferences="F:/host-policy/settings.xl";seed.manifests=manifests;seed.projects=2;
 for(size_t n=1;n<=allocations;++n){calls=opens=0;failAt=n;probing=1;same=xideNativeProtectionCreate(&seed,&error);probing=0;CHECK(!same&&error.code&&calls>=n&&!opens&&GetProcessHandleCount(GetCurrentProcess(),&after)&&handles==after);}failAt=0;
 seed.projects=256;seed.manifests=(const char* const*)(uintptr_t)1;CHECK(!xideNativeProtectionCreate(&seed,&error)&&error.code);seed.projects=2;const char* incomplete[]={manifests[0],NULL};seed.manifests=incomplete;CHECK(!xideNativeProtectionCreate(&seed,&error)&&error.code);seed.manifests=manifests;
 const char** many=calloc(255,sizeof(*many));CHECK(many);char names[255][100];for(size_t i=0;i<255;++i){snprintf(names[i],sizeof(names[i]),"F:/host-policy/project/%zu.xlp",i);many[i]=names[i];}seed.manifests=many;seed.projects=255;CHECK(!xideNativeProtectionCreate(&seed,&error)&&error.code==XIDE_MODEL_MEMORY);free(many);
 seed.manifests=manifests;seed.projects=2;seed.workspace=seed.storageDirectory;CHECK(!xideNativeProtectionCreate(&seed,&error)&&error.code==XIDE_MODEL_CONFLICT);seed.workspace=NULL;
 char* longPath=malloc(65537);CHECK(longPath);memset(longPath,'x',65536);longPath[65536]=0;seed.compiler=longPath;CHECK(!xideNativeProtectionCreate(&seed,&error)&&error.code);free(longPath);
 xideNativeProtectionRelease(policy);CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&handles==after);
 printf("PASS native host protection: %u checks; %zu observed allocation failures; complete metadata, Unicode ordinal dedup, safe source roots, no direct file opens, bounded failures and handle balance\n",checks,allocations);return 0;
}
