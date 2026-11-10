#include "services/rename_scope.h"
#ifdef XLS_COMPILER_XRT
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef XIDE_RENAME_SCOPE_ALLOC
#define XIDE_RENAME_SCOPE_ALLOC(bytes) xrtCalloc(1,(bytes))
#endif
typedef struct file_owner {wchar_t* key;size_t owner;} file_owner;
struct xide_rename_scope {
 xatomic32 references;xide_analysis_catalog* catalog;
 size_t projects,files,bytes;xls_context* contexts[64];file_owner index[64];
};
static int stopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static int charge(size_t* bytes,size_t add,size_t budget)
{if(*bytes>budget || add>budget-*bytes)return 0;*bytes+=add;return 1;}
static int order(const void* a,const void* b)
{const file_owner *x=a,*y=b;int n=xidePathNativeKeyCompare(x->key,y->key);return n ? n : (x->owner>y->owner)-(x->owner<y->owner);}
xide_rename_scope* xideRenameScopeRetain(xide_rename_scope* s)
{if(s)xrtAtomic32FetchAdd(&s->references,1,XMEMORY_RELAXED);return s;}
void xideRenameScopeRelease(xide_rename_scope* s)
{
 if(!s || xrtAtomic32FetchSub(&s->references,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;i<s->files;++i){xlsContextRelease(s->contexts[i]);free(s->index[i].key);}
 xideAnalysisCatalogDestroy(s->catalog);xrtFree(s);
}
xide_rename_scope* xideRenameScopeCreate(xide_analysis_catalog* catalog,xls_context* const* files,
 size_t count,size_t budget,int (*cancelled)(void*),void* user,enum xide_rename_scope_error* output)
{
 enum xide_rename_scope_error error=XIDE_RENAME_SCOPE_ARGUMENT;xide_rename_scope* s=NULL;
 size_t projects=xideAnalysisCatalogCount(catalog),bytes=sizeof(*s);budget=budget ? budget : XIDE_RENAME_SCOPE_BYTES;
 if(!catalog || (count && !files) || count>64 || projects>64-count)goto done;
 error=XIDE_RENAME_SCOPE_LIMIT;
 if(budget>XIDE_RENAME_SCOPE_BYTES || !charge(&bytes,xideAnalysisCatalogBytes(catalog),budget))goto done;
 if(stopped(cancelled,user)){error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
 s=XIDE_RENAME_SCOPE_ALLOC(sizeof(*s));if(!s){error=XIDE_RENAME_SCOPE_MEMORY;goto done;}
 xrtAtomic32Init(&s->references,1);s->catalog=xideAnalysisCatalogRetain(catalog);s->projects=projects;
 for(size_t i=0;i<count;++i){
  if(stopped(cancelled,user)){error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
  const xls_context_spec* spec=xlsContextSpec(files[i]);error=XIDE_RENAME_SCOPE_ARGUMENT;
  if(!spec || spec->kind!=XLS_CONTEXT_XLANG || !xrtPathIsAbs(spec->entry) || !xrtPathIsAbs(spec->directory) ||
     !xidePathHasExtension(spec->entry,".xl") || strcmp(spec->analysisProfile,XIDE_ANALYSIS_PROFILE))goto done;
  wchar_t* key=xidePathNativeWide(spec->entry);if(!key){error=XIDE_RENAME_SCOPE_MEMORY;goto done;}
  /* A file context must not smuggle unrelated members into a standalone role.
     Duplicate entry bindings are harmless; imported sources remain unowned. */
  int valid=1;
  for(size_t j=0;j<spec->lists[XLS_CONTEXT_MEMBERS].count && valid;++j){
   if(stopped(cancelled,user)){free(key);error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
   const char* path=spec->lists[XLS_CONTEXT_MEMBERS].items[j];
   if(!strcmp(path,spec->entry))continue;
   wchar_t* member=xrtPathIsAbs(path) ? xidePathNativeWide(path) : NULL;
   valid=member && !xidePathNativeKeyCompare(key,member);free(member);
  }
  if(!valid){free(key);goto done;}
  error=XIDE_RENAME_SCOPE_LIMIT;
  if(!charge(&bytes,xlsContextBytes(files[i]),budget) || !charge(&bytes,(wcslen(key)+1)*sizeof(wchar_t),budget)){free(key);goto done;}
  s->contexts[i]=xlsContextRetain(files[i]);s->index[i]=(file_owner){key,projects+i};++s->files;
 }
 qsort(s->index,s->files,sizeof(*s->index),order);s->bytes=bytes;
 error=stopped(cancelled,user) ? XIDE_RENAME_SCOPE_CANCELLED : XIDE_RENAME_SCOPE_OK;
done:
 if(error){xideRenameScopeRelease(s);s=NULL;}
 if(output)*output=error;
 return s;
}
size_t xideRenameScopeBytes(const xide_rename_scope* s){return s ? s->bytes : 0;}
size_t xideRenameScopeCount(const xide_rename_scope* s){return s ? s->projects+s->files : 0;}
const xls_context* xideRenameScopeAt(const xide_rename_scope* s,size_t at)
{return !s || at>=s->projects+s->files ? NULL : at<s->projects ? xideAnalysisCatalogAt(s->catalog,at) : s->contexts[at-s->projects];}
int xideRenameScopeOwnersNative(const xide_rename_scope* s,const wchar_t* key,uint64_t* owners)
{
 if(owners)*owners=0;
 if(!s || !key || !owners || !xideAnalysisCatalogOwnersNative(s->catalog,key,owners))return 0;
 size_t lo=0,hi=s->files;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(s->index[mid].key,key)<0)lo=mid+1;else hi=mid;}
 for(size_t i=lo;i<s->files && !xidePathNativeKeyCompare(s->index[i].key,key);++i)*owners|=UINT64_C(1)<<s->index[i].owner;
 return 1;
}
int xideRenameScopeOwners(const xide_rename_scope* s,const char* path,uint64_t* owners)
{
 if(owners)*owners=0;
 if(!s || !path || !owners || !xrtPathIsAbs(path))return 0;
 wchar_t* key=xidePathNativeWide(path);if(!key)return 0;
 int okay=xideRenameScopeOwnersNative(s,key,owners);free(key);return okay;
}
int xideRenameScopeTarget(const xide_rename_scope* s,const xide_rename_target* target,uint64_t* owners)
{
 if(owners)*owners=0;
 xide_rename_target_info at;
 if(!s || !owners || !xideRenameTargetInfo(target,&at))return 0;
 if(!(at.declaration.kind&1) || !xidePathHasExtension(at.declaration.path,".xl"))return 1;
 return xideRenameScopeOwners(s,at.declaration.path,owners);
}
#endif
