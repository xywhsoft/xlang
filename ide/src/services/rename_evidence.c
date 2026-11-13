#include "services/rename_evidence.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#define EVIDENCE_BYTES (256u*1024u*1024u)
typedef struct evidence_file {wchar_t* key;const xvalue* report;uint64_t providers;} evidence_file;
struct xide_rename_evidence {xatomic32 invalid;xide_rename_scope* scope;xide_language_input_guard** guards;size_t count,bytes;xarray files,touches;};
static const xvalue* get(const xvalue* v,const char* k){return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static const char* string(const xvalue* v){xstrview s={0};return v && xrtValueGetString(v,&s) && s.Data && s.Size && !memchr(s.Data,0,s.Size)?s.Data:NULL;}
static int stop(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static int fail(xide_model_error* e,const char* message){if(e){e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s",message);}return 0;}
static int charge(size_t* total,size_t add,size_t limit){if(*total>limit || add>limit-*total)return 0;*total+=add;return 1;}
typedef struct evidence_wire {size_t bytes;int (*cancelled)(void*);void* user;} evidence_wire;
static bool wireCount(xbytesview part,void* data){evidence_wire* s=data;return !stop(s->cancelled,s->user) && charge(&s->bytes,part.Size,7u*1024u*1024u);}
static int order(const void* a,const void* b){return xidePathNativeKeyCompare(((const evidence_file*)a)->key,((const evidence_file*)b)->key);}
static int touchOrder(const void* a,const void* b){return xidePathNativeKeyCompare(*(wchar_t* const*)a,*(wchar_t* const*)b);}
static int touch(xide_rename_evidence* e,const char* path,size_t budget)
{
 if(e->touches.Count>=163840)return 0;
 wchar_t* key=path?xidePathNativeWide(path):NULL;size_t capacity=e->touches.Capacity;
 if(!key || !charge(&e->bytes,(wcslen(key)+1)*sizeof(wchar_t),budget) || !xrtArrayPush(&e->touches,&key)){free(key);return 0;}
 return charge(&e->bytes,(e->touches.Capacity-capacity)*sizeof(key),budget);
}
static int sameReport(const xvalue* a,const xvalue* b)
{
 if(a==b)return 1;
 if(!xrtValueIs(a,XVALUE_OBJECT) || !xrtValueIs(b,XVALUE_OBJECT) || xrtValueCount(a)!=xrtValueCount(b))return 0;
 for(size_t i=0;i<xrtValueCount(a);++i){xstrview key={0};const xvalue* value=xrtValueObjectAt(a,i,&key);if(key.Size==4 && !memcmp(key.Data,"path",4))continue;if(!xrtValueEqual(value,xrtValueObjectGet(b,key)))return 0;}return 1;
}
static const evidence_file* find(const xide_rename_evidence* e,const wchar_t* key)
{
 size_t lo=0,hi=e->files.Count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(((evidence_file*)e->files.Data)[mid].key,key)<0)lo=mid+1;else hi=mid;}
 const evidence_file* row=lo<e->files.Count?(evidence_file*)e->files.Data+lo:NULL;return row && !xidePathNativeKeyCompare(row->key,key)?row:NULL;
}
void xideRenameEvidenceRelease(xide_rename_evidence* e)
{if(!e)return;if(e->guards)for(size_t i=0;i<e->count;++i)xideLanguageInputGuardRelease(e->guards[i]);xrtFree(e->guards);for(size_t i=0;i<e->files.Count;++i)free(((evidence_file*)e->files.Data)[i].key);for(size_t i=0;i<e->touches.Count;++i)free(((wchar_t**)e->touches.Data)[i]);xrtArrayUnit(&e->touches);xrtArrayUnit(&e->files);xideRenameScopeRelease(e->scope);xrtFree(e);}
size_t xideRenameEvidenceBytes(const xide_rename_evidence* e){return e?e->bytes:0;}
size_t xideRenameEvidenceFiles(const xide_rename_evidence* e){return e?e->files.Count:0;}
const xvalue* xideRenameEvidenceFile(const xide_rename_evidence* e,const char* path,uint64_t* providers)
{if(providers)*providers=0;wchar_t* key=e && path?xidePathNativeWide(path):NULL;if(!key)return NULL;const evidence_file* row=find(e,key);free(key);if(row && providers)*providers=row->providers;return row?row->report:NULL;}
int xideRenameEvidenceTouches(const xide_rename_evidence* e,const char* path)
{wchar_t* key=e && path?xidePathNativeWide(path):NULL;if(!key)return -1;size_t lo=0,hi=e->touches.Count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(((wchar_t**)e->touches.Data)[mid],key)<0)lo=mid+1;else hi=mid;}int found=lo<e->touches.Count && !xidePathNativeKeyCompare(((wchar_t**)e->touches.Data)[lo],key);free(key);return found;}
int xideRenameEvidenceCurrent(const xide_rename_evidence* e)
{return e && !xrtAtomic32Load(&e->invalid,XMEMORY_ACQUIRE);}
int xideRenameEvidenceValidate(xide_rename_evidence* e,int (*cancelled)(void*),void* user,xide_model_error* error)
{if(!xideRenameEvidenceCurrent(e) || stop(cancelled,user))return fail(error,"Rename evidence is invalidated or cancelled.");int okay=xideLanguageInputGuardsValidate(e->guards,e->count,cancelled,user,error);if(!okay || stop(cancelled,user))xrtAtomic32Store(&e->invalid,1,XMEMORY_RELEASE);return okay && !stop(cancelled,user);}
xide_rename_evidence* xideRenameEvidenceCreate(xide_rename_scope* scope,const xvalue* wire,const xide_language_proof_overlay* overlays,size_t overlayCount,size_t budget,int (*cancelled)(void*),void* user,xide_model_error* error)
{
 xide_rename_evidence* e=NULL;xls_context** expected=NULL;uint64_t* owners=NULL;size_t count=xrtValueCount(wire),scopes=xideRenameScopeCount(scope);uint64_t programs=0;
 budget=budget?budget:EVIDENCE_BYTES;
 if(!scope || !scopes || scopes>64 || !xrtValueIs(wire,XVALUE_ARRAY) || !count || count>65536 || overlayCount>64 || (overlayCount && !overlays) || budget>EVIDENCE_BYTES || stop(cancelled,user))goto failed;
 evidence_wire used={0,cancelled,user};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=7u*1024u*1024u;if(!xrtJsonWrite(wire,&config,wireCount,&used))goto failed;
 e=xrtCalloc(1,sizeof(*e));if(!e)goto failed;xrtAtomic32Init(&e->invalid,0);e->scope=xideRenameScopeRetain(scope);e->count=count;e->bytes=sizeof(*e);
 if(!charge(&e->bytes,xideRenameScopeBytes(scope),budget) || !charge(&e->bytes,count*(sizeof(*e->guards)+sizeof(*expected)+sizeof(*owners)),budget))goto failed;
 e->guards=xrtCalloc(count,sizeof(*e->guards));expected=xrtCalloc(count,sizeof(*expected));owners=xrtCalloc(count,sizeof(*owners));if(!e->guards || !expected || !owners || !xrtArrayInit(&e->files,sizeof(evidence_file)) || !xrtArrayInit(&e->touches,sizeof(wchar_t*)))goto failed;
 for(size_t i=0;i<count;++i){
  if(stop(cancelled,user))goto failed;
  xls_context* claimed=xideAnalysisContextRead(get(xrtValueArrayGet(wire,i),"context"),NULL,0);enum xide_rename_scope_error why;
  expected[i]=claimed?xideRenameScopeProvider(scope,claimed,owners+i,cancelled,user,&why):NULL;xlsContextRelease(claimed);if(!expected[i] || !owners[i])goto failed;
  const char* entry=xlsContextSpec(expected[i])->entry;
  for(size_t j=0;j<scopes;++j)if((owners[i]&(UINT64_C(1)<<j)) && xidePathLexicalEqual(entry,xlsContextSpec(xideRenameScopeAt(scope,j))->entry))programs|=UINT64_C(1)<<j;
 }
 if(!xideLanguageInputGuardsImport(wire,(const xls_context* const*)expected,count,overlays,overlayCount,e->guards,cancelled,user,error))goto failed;
 for(size_t i=0;i<count;++i){
  if(stop(cancelled,user) || !charge(&e->bytes,xideLanguageInputGuardReportBytes(e->guards[i]),budget))goto failed;
  const xvalue* files=xideLanguageInputGuardFiles(e->guards[i]);
  for(size_t j=0;j<xrtValueCount(files);++j){
   if(stop(cancelled,user) || e->files.Count>=65536)goto failed;
   const xvalue* file=xrtValueArrayGet(files,j);const char* path=string(get(file,"path"));evidence_file row={path?xidePathNativeWide(path):NULL,file,owners[i]};
   size_t capacity=e->files.Capacity;
   if(!row.key || !charge(&e->bytes,(wcslen(row.key)+1)*sizeof(wchar_t),budget) || !xrtArrayPush(&e->files,&row)){free(row.key);goto failed;}
   if(!charge(&e->bytes,(e->files.Capacity-capacity)*sizeof(row),budget))goto failed;
   if(!touch(e,path,budget))goto failed;
  }
  const xvalue* probes=xideLanguageInputGuardProbes(e->guards[i]);
  for(size_t j=0;j<xrtValueCount(probes);++j)if(stop(cancelled,user) || !touch(e,string(get(xrtValueArrayGet(probes,j),"path")),budget))goto failed;
 }
 if(!xrtArraySort(&e->files,order))goto failed;
 size_t kept=0;for(size_t i=0;i<e->files.Count;++i){
  if(stop(cancelled,user))goto failed;
  evidence_file* row=(evidence_file*)e->files.Data+i;
  if(kept && !xidePathNativeKeyCompare(((evidence_file*)e->files.Data)[kept-1].key,row->key)){
   evidence_file* previous=(evidence_file*)e->files.Data+kept-1;if(!sameReport(previous->report,row->report))goto failed;previous->providers|=row->providers;free(row->key);row->key=NULL;
  }else{if(i!=kept){((evidence_file*)e->files.Data)[kept]=*row;row->key=NULL;}++kept;}
 }
 if(!xrtArrayResize(&e->files,kept))goto failed;
 if(!xrtArraySort(&e->touches,touchOrder))goto failed;
 kept=0;for(size_t i=0;i<e->touches.Count;++i){if(stop(cancelled,user))goto failed;wchar_t** keys=(wchar_t**)e->touches.Data;if(kept && !xidePathNativeKeyCompare(keys[kept-1],keys[i])){free(keys[i]);keys[i]=NULL;}else{if(i!=kept){keys[kept]=keys[i];keys[i]=NULL;}++kept;}}
 if(!xrtArrayResize(&e->touches,kept))goto failed;
 uint64_t all=scopes==64?UINT64_MAX:(UINT64_C(1)<<scopes)-1;if(programs!=all)goto failed;
 for(size_t i=0;i<scopes;++i){
  const xls_context_spec* spec=xlsContextSpec(xideRenameScopeAt(scope,i));
  for(size_t j=0;j<=spec->lists[XLS_CONTEXT_MEMBERS].count;++j){
   if(stop(cancelled,user))goto failed;
   const char* path=j?spec->lists[XLS_CONTEXT_MEMBERS].items[j-1]:spec->entry;if(!xidePathHasExtension(path,".xl"))continue;
   wchar_t* key=xidePathNativeWide(path);const evidence_file* row=key?find(e,key):NULL;free(key);int64 kind=0;
   if(!row || !(row->providers&(UINT64_C(1)<<i)) || !xrtValueIs(get(row->report,"kind"),XVALUE_INT) || !xrtValueGetInt(get(row->report,"kind"),&kind) || !(kind&1))goto failed;
  }
 }
 for(size_t i=0;i<count;++i)xlsContextRelease(expected[i]);
 xrtFree(expected);xrtFree(owners);return e;
failed:
 if(expected)for(size_t i=0;i<count;++i)xlsContextRelease(expected[i]);
 xrtFree(expected);xrtFree(owners);xideRenameEvidenceRelease(e);fail(error,"Rename provider/source coverage, freshness, capacity or cancellation rejected the complete evidence.");return NULL;
}
