#include "services/analysis_catalog.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef struct binding { wchar_t* key;size_t owner; } binding;
struct xide_analysis_catalog {xatomic32 references;xls_context* projects[64];size_t count,bindingCount,selectedOwner,bytes;binding* bindings;char* package;
 xls_context* files[64];wchar_t* fileKeys[64];size_t fileCount;int complete;xide_analysis_catalog_guard guard;};
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
 for(size_t i=0;i<c->fileCount;++i){xlsContextRelease(c->files[i]);free(c->fileKeys[i]);}
 xrtFree(c->bindings);xrtFree(c->package);if(c->guard.value)c->guard.release(c->guard.value);xrtFree(c);
}
xide_analysis_catalog* xideAnalysisCatalogRetain(xide_analysis_catalog* c) {if(c) xrtAtomic32FetchAdd(&c->references,1,XMEMORY_RELAXED);return c;}
xide_analysis_catalog* xideAnalysisCatalogCreate(const xvalue* value,char* error,size_t capacity)
{
 if(error && capacity) *error=0;
 int64 schema=0;bool complete=true;const xvalue *projects=get(value,"projects"),*package=get(value,"packagePath"),*roots=get(value,"standaloneRoots");const char *selected=text(get(value,"selectedProject")),*raw=xrtValueType(package)==XVALUE_NULL ? NULL : text(package);
 if(xrtValueType(value)!=XVALUE_OBJECT || !xrtValueGetInt(get(value,"schema"),&schema) ||
    !((schema==1 && xrtValueCount(value)==4) || (schema==2 && xrtValueCount(value)==6 &&
      xrtValueType(roots)==XVALUE_ARRAY && xrtValueCount(roots)<=65536 && xrtValueType(get(value,"complete"))==XVALUE_BOOL && xrtValueGetBool(get(value,"complete"),&complete))) ||
    xrtValueType(projects)!=XVALUE_ARRAY || xrtValueCount(projects)>64 || !selected || !package || (xrtValueType(package)!=XVALUE_NULL && !raw)) {
  fail(error,capacity,"Invalid analysis catalog schema or capacity.");return NULL;
 }
 xide_analysis_catalog* c=xrtCalloc(1,sizeof(*c));if(!c) goto memory;
 xrtAtomic32Init(&c->references,1);c->selectedOwner=SIZE_MAX;c->complete=complete;c->package=raw ? xrtStrDup(raw) : NULL;if(raw && !c->package) goto memory;
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
 c->bytes=sizeof(*c)+bindings*sizeof(*c->bindings)+(c->package ? strlen(c->package)+1 : 0);
 for(size_t i=0;i<c->count;++i)c->bytes+=xlsContextBytes(c->projects[i]);
 for(size_t i=0;i<c->bindingCount;++i)c->bytes+=(wcslen(c->bindings[i].key)+1)*sizeof(wchar_t);
 /* Metadata only: never infer roots by walking directories or opening files.
    Validate even redundant input, then union by the existing native path key. */
 size_t labels=0,contextBytes=0;
 for(size_t i=0;i<c->count;++i)contextBytes+=xlsContextBytes(c->projects[i]);
 for(size_t i=0;schema==2 && i<xrtValueCount(roots);++i){
  const char* path=text(xrtValueArrayGet(roots,i));uint64_t owners=0;
  if(!path || !xrtPathIsAbs(path) || !xidePathHasExtension(path,".xl") || !xrtUtf8Valid(sv(path),NULL) ||
     strlen(path)+1>16u*1024u*1024u-labels)goto invalid;
  labels+=strlen(path)+1;
  wchar_t* key=xidePathNativeWide(path);if(!key)goto memory;
  if(!xideAnalysisCatalogOwnersNative(c,key,&owners)){free(key);goto invalid;}
  int duplicate=owners!=0;
  for(size_t j=0;j<c->fileCount && !duplicate;++j)duplicate=!xidePathNativeKeyCompare(key,c->fileKeys[j]);
  if(duplicate){free(key);continue;}
  if(c->count+c->fileCount==64){free(key);goto invalid;}
  xls_context* file=xideAnalysisCatalogFile(c,path);if(!file){free(key);goto memory;}
  size_t add=xlsContextBytes(file),keyBytes=(wcslen(key)+1)*sizeof(wchar_t);
  if(add>16u*1024u*1024u-contextBytes || keyBytes>64u*1024u*1024u-bytes){free(key);xlsContextRelease(file);goto invalid;}
  contextBytes+=add;bytes+=keyBytes;c->bytes+=add+keyBytes;
  c->files[c->fileCount]=file;c->fileKeys[c->fileCount++]=key;
 }
 return c;
invalid:fail(error,capacity,"Invalid project analysis identity, selection, paths or capacity.");goto failed;
memory:fail(error,capacity,"Cannot allocate prepared analysis catalog.");
failed:xideAnalysisCatalogDestroy(c);return NULL;
}
xide_analysis_catalog* xideAnalysisCatalogEmpty(const char* package,char* error,size_t capacity)
{
 xvalue* value=xrtValueObject();int okay=value&&xrtValueObjectSetNew(value,sv("schema"),xrtValueInt(1))&&xrtValueObjectSetNew(value,sv("selectedProject"),xrtValueString(sv("")))&&xrtValueObjectSetNew(value,sv("packagePath"),package?xrtValueString(sv(package)):xrtValueNull())&&xrtValueObjectSetNew(value,sv("projects"),xrtValueArray());
 xide_analysis_catalog* c=okay?xideAnalysisCatalogCreate(value,error,capacity):NULL;xrtValueRelease(value);if(!okay)fail(error,capacity,"Cannot allocate empty analysis catalog.");return c;
}
xide_analysis_catalog* xideAnalysisCatalogCreateGuarded(const xvalue* value,const xide_analysis_catalog_guard* guard,char* error,size_t capacity)
{
 if(!guard || !guard->value || !guard->retain || !guard->release || !guard->validate){fail(error,capacity,"Invalid external analysis catalog evidence.");return NULL;}
 xide_analysis_catalog* c=xideAnalysisCatalogCreate(value,error,capacity);if(!c)return NULL;
 if(guard->bytes>SIZE_MAX-c->bytes){fail(error,capacity,"Analysis catalog evidence accounting overflow.");xideAnalysisCatalogDestroy(c);return NULL;}
 void* retained=guard->retain(guard->value);if(!retained){fail(error,capacity,"Cannot retain analysis catalog evidence.");xideAnalysisCatalogDestroy(c);return NULL;}
 c->guard=*guard;c->guard.value=retained;c->bytes+=guard->bytes;return c;
}
int xideAnalysisCatalogValidate(const xide_analysis_catalog* c,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
 if(error&&capacity)*error=0;
 if(cancelled&&cancelled(user)){fail(error,capacity,"Analysis catalog evidence validation cancelled.");return 0;}
 return !c || !c->guard.value || c->guard.validate(c->guard.value,cancelled,user,error,capacity);
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
int xideAnalysisCatalogOwnersNative(const xide_analysis_catalog* c,const wchar_t* key,uint64_t* owners)
{
 if(owners)*owners=0;
 if(!c || !key || !owners)return 0;
 size_t low=0,high=c->bindingCount;
 while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(c->bindings[mid].key,key)<0)low=mid+1;else high=mid;}
 for(size_t i=low;i<c->bindingCount && !xidePathNativeKeyCompare(c->bindings[i].key,key);++i)*owners|=UINT64_C(1)<<c->bindings[i].owner;
 return 1;
}
size_t xideAnalysisCatalogBytes(const xide_analysis_catalog* c){return c ? c->bytes : 0;}
int xideAnalysisCatalogPrefers(const xide_analysis_catalog* c,const xls_context* context)
{return c && c->selectedOwner!=SIZE_MAX && context && (c->projects[c->selectedOwner]==context || xlsContextEqual(c->projects[c->selectedOwner],context));}
const char* xideAnalysisCatalogPackagePath(const xide_analysis_catalog* c) {return c ? c->package : NULL;}
size_t xideAnalysisCatalogCount(const xide_analysis_catalog* c) {return c ? c->count : 0;}
const xls_context* xideAnalysisCatalogAt(const xide_analysis_catalog* c,size_t index) {return c && index<c->count ? c->projects[index] : NULL;}
size_t xideAnalysisCatalogFileCount(const xide_analysis_catalog* c){return c ? c->fileCount : 0;}
const xls_context* xideAnalysisCatalogFileAt(const xide_analysis_catalog* c,size_t i){return c && i<c->fileCount ? c->files[i] : NULL;}
int xideAnalysisCatalogComplete(const xide_analysis_catalog* c){return c && c->complete;}
xls_context* xideAnalysisCatalogFile(const xide_analysis_catalog* c,const char* path)
{
 if(!c || !path || !xrtPathIsAbs(path) || !xidePathHasExtension(path,".xl")) return NULL;
 if(c->fileCount){wchar_t* key=xidePathNativeWide(path);if(!key)return NULL;
  for(size_t i=0;i<c->fileCount;++i)if(!xidePathNativeKeyCompare(key,c->fileKeys[i])){free(key);return xlsContextRetain(c->files[i]);}
  free(key);}
 char* directory=xidePathDirectory(path);if(!directory) return NULL;
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=c->package};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){&path,1};xls_context* result=xlsContextCreate(&spec,16u*1024u*1024u,NULL);free(directory);return result;
}
