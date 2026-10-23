#include "services/analysis_catalog.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef struct binding { wchar_t* key;size_t owner; } binding;
struct xide_analysis_catalog {xatomic32 references;xls_context* projects[64];size_t count,bindingCount,selectedOwner;binding* bindings;char* package;};
static xstrview sv(const char* s) {return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key) {return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL;}
static const char* text(const xvalue* v) {xstrview s={0};return xrtValueGetString(v,&s) && s.Size<=32767 && (!s.Size || (s.Data && !memchr(s.Data,0,s.Size))) ? (s.Data ? s.Data : "") : NULL;}
static void fail(char* error,size_t capacity,const char* message) {if(error && capacity) snprintf(error,capacity,"%s",message);}
static int compare(const void* a,const void* b)
{const binding *x=a,*y=b;int result=xidePathNativeKeyCompare(x->key,y->key);return result ? result : (x->owner>y->owner)-(x->owner<y->owner);}
void xideAnalysisCatalogDestroy(xide_analysis_catalog* c)
{
 if(!c || xrtAtomic32FetchSub(&c->references,1,XMEMORY_ACQ_REL)!=1) return;
 for(size_t i=0;i<c->bindingCount;++i) free(c->bindings[i].key);
 for(size_t i=0;i<c->count;++i) xlsContextRelease(c->projects[i]);
 xrtFree(c->bindings);xrtFree(c->package);xrtFree(c);
}
xide_analysis_catalog* xideAnalysisCatalogRetain(xide_analysis_catalog* c) {if(c) xrtAtomic32FetchAdd(&c->references,1,XMEMORY_RELAXED);return c;}
xide_analysis_catalog* xideAnalysisCatalogCreate(const xvalue* value,char* error,size_t capacity)
{
 if(error && capacity) *error=0;
 int64 schema=0;const xvalue *projects=get(value,"projects"),*package=get(value,"packagePath");const char *selected=text(get(value,"selectedProject")),*raw=xrtValueType(package)==XVALUE_NULL ? NULL : text(package);
 if(xrtValueType(value)!=XVALUE_OBJECT || xrtValueCount(value)!=4 || !xrtValueGetInt(get(value,"schema"),&schema) || schema!=1 ||
    xrtValueType(projects)!=XVALUE_ARRAY || xrtValueCount(projects)>64 || !selected || !package || (xrtValueType(package)!=XVALUE_NULL && !raw)) {
  fail(error,capacity,"Invalid analysis catalog schema or capacity.");return NULL;
 }
 xide_analysis_catalog* c=xrtCalloc(1,sizeof(*c));if(!c) goto memory;
 xrtAtomic32Init(&c->references,1);c->selectedOwner=SIZE_MAX;c->package=raw ? xrtStrDup(raw) : NULL;if(raw && !c->package) goto memory;
 size_t count=xrtValueCount(projects),bindings=0,bytes=0;int selectedFound=!*selected;
 for(size_t i=0;i<count;++i) {
  xls_context* context=xideAnalysisContextRead(xrtValueArrayGet(projects,i),error,capacity);if(!context) goto failed;
  c->projects[c->count++]=context;const xls_context_spec* spec=xlsContextSpec(context);
  if(spec->kind!=XLS_CONTEXT_PROJECT || !xrtPathIsAbs(spec->entry) || !xrtPathIsAbs(spec->directory) || !xrtPathIsAbs(spec->project) ||
     !xidePathHasExtension(spec->entry,".xl") || strcmp(spec->analysisProfile,XIDE_ANALYSIS_PROFILE) ||
     (spec->packagePath && strlen(spec->packagePath)>32767) || spec->lists[XLS_CONTEXT_MEMBERS].count>=65536-bindings || xlsContextBytes(context)>16u*1024u*1024u-bytes) goto invalid;
  for(size_t j=0;j<i;++j) if(xidePathLexicalEqual(spec->project,xlsContextSpec(c->projects[j])->project)) goto invalid;
  if(*selected && xidePathLexicalEqual(selected,spec->project)) {selectedFound=1;c->selectedOwner=i;}
  bindings+=1+spec->lists[XLS_CONTEXT_MEMBERS].count;bytes+=xlsContextBytes(context);
 }
 if(!selectedFound) goto invalid;
 if(bindings && !(c->bindings=xrtCalloc(bindings,sizeof(*c->bindings)))) goto memory;
 bytes=0;
 for(size_t i=0;i<c->count;++i) {
  const xls_context_spec* spec=xlsContextSpec(c->projects[i]);
  for(size_t j=0;j<=spec->lists[XLS_CONTEXT_MEMBERS].count;++j) {
   const char* path=j ? spec->lists[XLS_CONTEXT_MEMBERS].items[j-1] : spec->entry;
   wchar_t* key=xrtPathIsAbs(path) ? xidePathNativeWide(path) : NULL;if(!key) goto invalid;
   size_t charge=(wcslen(key)+1)*sizeof(wchar_t);if(charge>64u*1024u*1024u-bytes) {free(key);goto invalid;}
   c->bindings[c->bindingCount++]=(binding){key,i};bytes+=charge;
  }
 }
 if(c->bindingCount) qsort(c->bindings,c->bindingCount,sizeof(*c->bindings),compare);
 size_t kept=0;
 for(size_t i=0;i<c->bindingCount;++i) {
  if(kept && c->bindings[kept-1].owner==c->bindings[i].owner && !xidePathNativeKeyCompare(c->bindings[kept-1].key,c->bindings[i].key)) free(c->bindings[i].key);
  else c->bindings[kept++]=c->bindings[i];
 }
 c->bindingCount=kept;
 return c;
invalid:fail(error,capacity,"Invalid project analysis identity, selection, paths or capacity.");goto failed;
memory:fail(error,capacity,"Cannot allocate prepared analysis catalog.");
failed:xideAnalysisCatalogDestroy(c);return NULL;
}
enum xide_analysis_binding xideAnalysisCatalogFindNative(const xide_analysis_catalog* c,const wchar_t* key,const xls_context** context)
{
 if(context) *context=NULL;
 if(!c || !key || !context) return XIDE_ANALYSIS_ERROR;
 size_t low=0,high=c->bindingCount;
 while(low<high) {size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(c->bindings[mid].key,key)<0) low=mid+1;else high=mid;}
 size_t chosen=SIZE_MAX;int ambiguous=0,selected=0;
 for(size_t i=low;i<c->bindingCount && !xidePathNativeKeyCompare(c->bindings[i].key,key);++i) {
  size_t owner=c->bindings[i].owner;
  if(c->selectedOwner==owner) {chosen=owner;selected=1;break;}
  if(chosen!=SIZE_MAX && chosen!=owner) ambiguous=1;
  chosen=owner;
 }
 if(!selected && ambiguous) return XIDE_ANALYSIS_AMBIGUOUS;
 if(chosen==SIZE_MAX) return XIDE_ANALYSIS_UNOWNED;
 *context=c->projects[chosen];return XIDE_ANALYSIS_PROJECT;
}
enum xide_analysis_binding xideAnalysisCatalogFind(const xide_analysis_catalog* c,const char* path,const xls_context** context)
{
 if(context) *context=NULL;
 if(!c || !path || !context || !xrtPathIsAbs(path)) return XIDE_ANALYSIS_ERROR;
 wchar_t* key=xidePathNativeWide(path);if(!key) return XIDE_ANALYSIS_ERROR;
 enum xide_analysis_binding result=xideAnalysisCatalogFindNative(c,key,context);free(key);return result;
}
int xideAnalysisCatalogPrefers(const xide_analysis_catalog* c,const xls_context* context)
{return c && c->selectedOwner!=SIZE_MAX && context && (c->projects[c->selectedOwner]==context || xlsContextEqual(c->projects[c->selectedOwner],context));}
const char* xideAnalysisCatalogPackagePath(const xide_analysis_catalog* c) {return c ? c->package : NULL;}
size_t xideAnalysisCatalogCount(const xide_analysis_catalog* c) {return c ? c->count : 0;}
xls_context* xideAnalysisCatalogFile(const xide_analysis_catalog* c,const char* path)
{
 if(!c || !path || !xrtPathIsAbs(path) || !xidePathHasExtension(path,".xl")) return NULL;
 char* directory=xidePathDirectory(path);if(!directory) return NULL;
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=c->package};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){&path,1};xls_context* result=xlsContextCreate(&spec,16u*1024u*1024u,NULL);free(directory);return result;
}
