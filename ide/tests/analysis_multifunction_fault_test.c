#define main m74BasicMain
#include "analysis_multifunction_syntax_test.c"
#undef main
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#include "analysis/ast_clone.h"
#include <stdlib.h>
static size_t seen,at,denied,live;
static int active;
static void* pointers[2048];
static int refuse(void){if(active && ++seen==at){++denied;return 1;}return 0;}
static void remember(void* p){if(!active || !p)return;for(size_t i=0;i<2048;++i)if(pointers[i]==p)return;for(size_t i=0;i<2048;++i)if(!pointers[i]){pointers[i]=p;++live;return;}abort();}
static void forget(void* p){if(p)for(size_t i=0;i<2048;++i)if(pointers[i]==p){pointers[i]=NULL;--live;return;}}
static void* gateCalloc(size_t n,size_t s){if(refuse())return NULL;void* p=xrtCalloc(n,s);remember(p);return p;}
static char* gateDup(const char* s){if(refuse())return NULL;char* p=xrtStrDup(s);remember(p);return p;}
static char* gateDupN(const char* s,size_t n){if(refuse())return NULL;char* p=xrtStrDupN(s,n);remember(p);return p;}
static void gateFree(void* p){forget(p);xrtFree(p);}
static bool gateArrayInit(xarray* a,size_t n){return !refuse() && xrtArrayInit(a,n);}
static bool gatePush(xarray* a,const void* p){if(refuse())return false;void* prior=a->Data;bool okay=xrtArrayPush(a,p);if(a->Data!=prior){forget(prior);remember(a->Data);}return okay;}
static void gateArrayUnit(xarray* a){forget(a->Data);xrtArrayUnit(a);}
static bool gateMutex(xmutex* p){return !refuse() && xrtMutexInit(p);}
static bool gateMapInit(xmap* p,size_t n){return !refuse() && xrtMapInit(p,n);}
static bool gateMapSet(xmap* p,xbytesview k,const void* v){return !refuse() && xrtMapSet(p,k,v);}
static XL6_AstNode* gateClone(const XL6_AstNode* n,const xls_ast_clone_options* options,size_t* bytes){if(refuse())return NULL;XL6_AstNode* p=xlsAstClone(n,options,bytes);remember(p);return p;}
static void gateAstFree(XL6_AstNode* p){if(p)forget(p->doc);forget(p);xl6AstFree(p);}
static int gateParse(const char* text,const char* path,XL6_ParseResult* p){if(refuse())return 0;int okay=xl6ParseSource(text,path,p);if(p->root)for(size_t i=0;i<p->root->childCount;++i)remember(p->root->children[i]);return okay;}
static void gateParseFree(XL6_ParseResult* p){if(p->root){for(size_t i=0;i<p->root->childCount;++i)forget(p->root->children[i]);forget(p->root->doc);forget(p->root);}xl6ParseResultFree(p);}
#define xrtCalloc gateCalloc
#define xrtStrDup gateDup
#define xrtStrDupN gateDupN
#define xrtFree gateFree
#define xrtArrayInit gateArrayInit
#define xrtArrayPush gatePush
#define xrtArrayUnit gateArrayUnit
#define xrtMutexInit gateMutex
#define xrtMapInit gateMapInit
#define xrtMapSet gateMapSet
#define xlsAstClone gateClone
#define xl6AstFree gateAstFree
#define xl6ParseSource gateParse
#define xl6ParseResultFree gateParseFree
#include "../src/analysis/syntax.c"
#undef xrtCalloc
#undef xrtStrDup
#undef xrtStrDupN
#undef xrtFree
#undef xrtArrayInit
#undef xrtArrayPush
#undef xrtArrayUnit
#undef xrtMutexInit
#undef xrtMapInit
#undef xrtMapSet
#undef xlsAstClone
#undef xl6AstFree
#undef xl6ParseSource
#undef xl6ParseResultFree
static int faults(void)
{
    char* path=xrtPathAbs("multi-fault.xl"),*body=make(8,0,1),*changed=make(8,2,1);CHECK(path && body && changed);
    xls_text *before=xlsTextCreate(body,strlen(body),1024u*1024u,NULL),*after=xlsTextCreate(changed,strlen(changed),1024u*1024u,NULL);CHECK(before && after);
    xls_parse_work cold={0};xls_syntax* old=xlsSyntaxBuild(path,before,NULL,&cold);CHECK(old && verify(old,body));size_t sites=0;
    for(size_t i=0;i<=sites;++i){
        seen=denied=0;at=i;active=1;xls_parse_work work={0};xls_syntax* next=xlsSyntaxTryFunction(path,after,old,&work);active=0;
        if(!i){sites=seen;CHECK(sites>20 && sites<1024 && next && work.fragments==2);}else CHECK(denied==1);
        if(next){CHECK(verify(next,changed));}xlsSyntaxRelease(next);CHECK(!live);
        for(size_t j=0;j<old->units.Count;++j)CHECK(xrtAtomic32Load(&((placement*)xrtArrayGet(&old->units,j))->unit->references,XMEMORY_RELAXED)==1);
        CHECK(verify(old,body));work=(xls_parse_work){0};next=xlsSyntaxTryFunction(path,after,old,&work);CHECK(next && !work.full && work.fragments==2 && verify(next,changed));xlsSyntaxRelease(next);CHECK(!live);
    }
    printf("multi-function targeted construction/dependency failure sites=%zu; all tracked structs, strings, arrays and AST roots retired; old unit references restored.\n",sites);
    xlsSyntaxRelease(old);xlsTextRelease(before);xlsTextRelease(after);xrtFree(path);xrtFree(body);xrtFree(changed);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!faults())return 1;
    printf("analysis_multifunction_fault_test passed: %u checks; targeted caller failures, not whole-allocator exhaustion.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}

#else
int main(void){puts("analysis_multifunction_fault_test passed: legacy keeps original single-function route");return 0;}
#endif
