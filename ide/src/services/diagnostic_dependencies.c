#include "services/diagnostic_dependencies.h"
#include "protocol/diagnostic_paths.h"
#include "platform/path.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SET_LIMIT 8192u
#define PATH_LIMIT (64u*(512u+2048u))
#define PART_BYTES (256u*1024u)
#define SET_BYTES (32u*1024u*1024u)
#define ALL_BYTES (64u*1024u*1024u)
#define KEY_BYTES (16u*1024u*1024u)
typedef struct dependency_path { xvalue* value;wchar_t* key; } dependency_path;
struct xide_diagnostic_dependency {
 xatomic32 references;char id[65];xarray paths;
 size_t slot,parts,next,encodedBytes,bindings;int complete;
};
typedef struct binding {wchar_t* key;xide_diagnostic_dependency* dependency;} binding;
struct xide_diagnostic_dependencies {
 xide_dependency_limits limits;xide_dependency_stats stats;
 xmap ids;xarray entries,bindings;char error[256];
};
static xstrview sv(const char* s) {return xrtStrView(s);}
static const xvalue* get(const xvalue* value,const char* name)
{return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,sv(name)) : NULL;}
static const char* string(const xvalue* value,size_t limit)
{
 xstrview text={0};return xrtValueGetString(value,&text) && text.Size && text.Size<=limit &&
  !memchr(text.Data,0,text.Size) && xrtUtf8Valid(text,NULL) ? text.Data : NULL;
}
static int digestId(const char* id)
{if(!id || strlen(id)!=64) return 0;for(size_t i=0;i<64;++i) if(!((id[i]>='0' && id[i]<='9') || (id[i]>='a' && id[i]<='f'))) return 0;return 1;}
static int stopped(int (*cancelled)(void*),void* user) {return cancelled && cancelled(user);}
static int integer(const xvalue* value,int64* out)
{return value && xrtValueType(value)==XVALUE_INT && xrtValueGetInt(value,out);}
static int fail(xide_diagnostic_dependencies* d,const char* message)
{if(d) snprintf(d->error,sizeof(d->error),"%s",message);return 0;}
static xide_diagnostic_dependency* find(const xide_diagnostic_dependencies* d,const char* id)
{
 if(!d || !digestId(id)) return NULL;
 xide_diagnostic_dependency* const* found=xrtMapConstGet(&d->ids,(xbytesview){(const unsigned char*)id,64});return found ? *found : NULL;
}
xide_diagnostic_dependency* xideDiagnosticDependencyRetain(const xide_diagnostic_dependency* value)
{xide_diagnostic_dependency* p=(xide_diagnostic_dependency*)value;if(p) xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xideDiagnosticDependencyRelease(xide_diagnostic_dependency* p)
{
 if(!p || xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)!=1) return;
 dependency_path* paths=(dependency_path*)p->paths.Data;
 for(size_t i=0;i<p->paths.Count;++i) {free(paths[i].key);xrtValueRelease(paths[i].value);}
 xrtArrayUnit(&p->paths);xrtFree(p);
}
const char* xideDiagnosticDependencyId(const xide_diagnostic_dependency* p) {return p && p->complete ? p->id : NULL;}
size_t xideDiagnosticDependencyCount(const xide_diagnostic_dependency* p) {return p && p->complete ? p->paths.Count : 0;}
const char* xideDiagnosticDependencyPath(const xide_diagnostic_dependency* p,size_t index)
{
 xstrview text={0};return p && p->complete && index<p->paths.Count &&
  xrtValueGetString(((dependency_path*)p->paths.Data)[index].value,&text) ? text.Data : NULL;
}
int xideDiagnosticDependencyContainsKey(const xide_diagnostic_dependency* p,const wchar_t* key)
{
 if(!p || !p->complete || !key) return 0;
 const dependency_path* paths=(const dependency_path*)p->paths.Data;size_t low=0,high=p->paths.Count;
 while(low<high) {size_t mid=low+(high-low)/2;int cmp=xidePathNativeKeyCompare(paths[mid].key,key);if(cmp<0) low=mid+1;else if(cmp>0) high=mid;else return 1;}return 0;
}
const xide_diagnostic_dependency* xideDiagnosticDependenciesFind(const xide_diagnostic_dependencies* d,const char* id)
{const xide_diagnostic_dependency* p=find(d,id);return p && p->complete ? p : NULL;}
xide_diagnostic_dependencies* xideDiagnosticDependenciesCreate(const xide_dependency_limits* limits)
{
 xide_dependency_limits l=limits ? *limits : (xide_dependency_limits){SET_LIMIT,ALL_BYTES,PATH_LIMIT,SET_LIMIT,KEY_BYTES};
 if(!l.dictionaries || l.dictionaries>SET_LIMIT || !l.encodedBytes || l.encodedBytes>ALL_BYTES ||
    !l.pathsPerDictionary || l.pathsPerDictionary>PATH_LIMIT || !l.bindings || l.bindings>SET_LIMIT || !l.bindingKeyBytes || l.bindingKeyBytes>KEY_BYTES) return NULL;
 xide_diagnostic_dependencies* d=xrtCalloc(1,sizeof(*d));if(!d) return NULL;d->limits=l;
 if(!xrtMapInit(&d->ids,sizeof(xide_diagnostic_dependency*)) || !xrtArrayInit(&d->entries,sizeof(xide_diagnostic_dependency*)) ||
    !xrtArrayInit(&d->bindings,sizeof(binding))) {xideDiagnosticDependenciesDestroy(d);return NULL;}return d;
}
void xideDiagnosticDependenciesDestroy(xide_diagnostic_dependencies* d)
{
 if(!d) return;
 for(size_t i=0;i<d->bindings.Count;++i) free(((binding*)d->bindings.Data)[i].key);
 for(size_t i=0;i<d->entries.Count;++i) xideDiagnosticDependencyRelease(((xide_diagnostic_dependency**)d->entries.Data)[i]);
 xrtArrayUnit(&d->bindings);xrtArrayUnit(&d->entries);xrtMapUnit(&d->ids);xrtFree(d);
}
void xideDiagnosticDependenciesStats(const xide_diagnostic_dependencies* d,xide_dependency_stats* stats)
{if(stats) *stats=d ? d->stats : (xide_dependency_stats){0};}
const char* xideDiagnosticDependenciesError(const xide_diagnostic_dependencies* d) {return d ? d->error : "No diagnostic dependency registry.";}
typedef struct measure {size_t bytes;int (*cancelled)(void*);void* user;} measure;
static bool countBytes(xbytesview bytes,void* user)
{measure* m=user;if(bytes.Size>SIZE_MAX-m->bytes || stopped(m->cancelled,m->user)) return false;m->bytes+=bytes.Size;return true;}
static size_t encodedBytes(const xvalue* params,int (*cancelled)(void*),void* user)
{
 xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=PART_BYTES;measure m={0,cancelled,user};
 return xrtJsonWrite(params,&config,countBytes,&m) ? m.bytes : SIZE_MAX;
}
static int matchesDigest(const xide_diagnostic_dependency* p,const xarray* staged,const char* id,int (*cancelled)(void*),void* user)
{
 xvalue* paths=xrtValueArray();int okay=paths!=NULL;const xarray* lists[]={p ? &p->paths : NULL,staged};size_t at=0;
 for(size_t i=0;okay && i<2;++i) if(lists[i]) for(size_t j=0;okay && j<lists[i]->Count;++j) {
  const dependency_path* path=(const dependency_path*)lists[i]->Data+j;
  okay=(at++%16 || !stopped(cancelled,user)) && xrtValueArrayAppend(paths,path->value);
 }
 char digest[65];okay=okay && xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,id) && !stopped(cancelled,user);xrtValueRelease(paths);return okay;
}
enum xide_dependency_message xideDiagnosticDependenciesMessage(xide_diagnostic_dependencies* d,const xvalue* params,int (*cancelled)(void*),void* user)
{
 if(!d) return XIDE_DEPENDENCY_ERROR;
 d->error[0]=0;
 if(stopped(cancelled,user)) return XIDE_DEPENDENCY_CANCELLED;
 int64 schema=0,part=0,parts=0;bool removed=false;const char* id=string(get(params,"id"),64);
 if(!digestId(id) || !integer(get(params,"schema"),&schema) || schema!=1 || !xrtValueGetBool(get(params,"removed"),&removed)) return fail(d,"Invalid dependency envelope.");
 xide_diagnostic_dependency* p=find(d,id);
 if(removed) {
  if(xrtValueCount(params)!=3 || !p || !p->complete || p->bindings) return fail(d,"Dependency removal has missing data or live bindings.");
  if(!xrtMapRemove(&d->ids,(xbytesview){(const unsigned char*)id,64})) return fail(d,"Cannot remove dependency index.");
  ((xide_diagnostic_dependency**)d->entries.Data)[p->slot]=NULL;--d->stats.dictionaries;d->stats.encodedBytes-=p->encodedBytes;
  xideDiagnosticDependencyRelease(p);return XIDE_DEPENDENCY_REMOVED;
 }
 const xvalue* paths=get(params,"paths");
 if(xrtValueCount(params)!=6 || !integer(get(params,"part"),&part) || !integer(get(params,"parts"),&parts) ||
    part<0 || parts<1 || parts>65536 || part>=parts || !paths || xrtValueType(paths)!=XVALUE_ARRAY ||
    (p ? p->complete || (size_t)part!=p->next || (size_t)parts!=p->parts : part!=0)) return fail(d,"Invalid dependency part sequence.");
 if(!p && d->stats.dictionaries==d->limits.dictionaries) return fail(d,"Dependency dictionary count limit exceeded.");
 size_t count=xrtValueCount(paths),previous=p ? p->paths.Count : 0;
 if(count>d->limits.pathsPerDictionary-previous) return fail(d,"Dependency path count limit exceeded.");
 size_t bytes=encodedBytes(params,cancelled,user),retained=p ? p->encodedBytes : 0;
 if(stopped(cancelled,user)) return XIDE_DEPENDENCY_CANCELLED;
 if(bytes==SIZE_MAX || bytes>SET_BYTES-retained || bytes>d->limits.encodedBytes-d->stats.encodedBytes) return fail(d,"Dependency encoding or retained byte limit exceeded.");
 xarray staged={0};xide_diagnostic_dependency* fresh=NULL;enum xide_dependency_message result=XIDE_DEPENDENCY_ERROR;
 if(!xrtArrayInit(&staged,sizeof(dependency_path)) || !xrtArrayReserve(&staged,count)) {fail(d,"Cannot allocate dependency part.");goto done;}
 const wchar_t* last=previous ? ((dependency_path*)p->paths.Data)[previous-1].key : NULL;
 for(size_t i=0;i<count;++i) {
  if(i%16==0 && stopped(cancelled,user)) {result=XIDE_DEPENDENCY_CANCELLED;goto done;}
  const xvalue* value=xrtValueArrayGet(paths,i);const char* text=string(value,65535);wchar_t* key=text && xrtPathIsAbs(text) ? xidePathNativeWide(text) : NULL;
  if(!key || (last && xidePathNativeKeyCompare(last,key)>=0)) {free(key);fail(d,"Dependency paths must be absolute, strictly ordered and unique.");goto done;}
  dependency_path item={xrtValueRetain((xvalue*)value),key};
  if(!xrtArrayPush(&staged,&item)) {free(key);xrtValueRelease(item.value);fail(d,"Cannot stage dependency path.");goto done;}
  last=key;
 }
 int complete=part+1==parts;
 if(complete && !matchesDigest(p,&staged,id,cancelled,user)) {if(stopped(cancelled,user)) result=XIDE_DEPENDENCY_CANCELLED;else fail(d,"Dependency digest mismatch or allocation failure.");goto done;}
 if(!p) {
  fresh=xrtCalloc(1,sizeof(*fresh));
  if(!fresh) {fail(d,"Cannot allocate dependency set.");goto done;}
  xrtAtomic32Init(&fresh->references,1);memcpy(fresh->id,id,65);fresh->parts=(size_t)parts;
  if(!xrtArrayInit(&fresh->paths,sizeof(dependency_path))) {fail(d,"Cannot allocate dependency paths.");goto done;}
  p=fresh;
 }
 if(!xrtArrayReserve(&p->paths,previous+count)) {fail(d,"Cannot retain dependency paths.");goto done;}
 if(fresh) {
  size_t slot=0;while(slot<d->entries.Count && ((xide_diagnostic_dependency**)d->entries.Data)[slot]) ++slot;
  if((slot==d->entries.Count && !xrtArrayReserve(&d->entries,slot+1)) || stopped(cancelled,user)) {if(stopped(cancelled,user)) result=XIDE_DEPENDENCY_CANCELLED;else fail(d,"Cannot retain dependency slot.");goto done;}
  if(!xrtMapSet(&d->ids,(xbytesview){(const unsigned char*)id,64},&p)) {fail(d,"Cannot retain dependency index.");goto done;}
  if(slot==d->entries.Count) {memcpy((unsigned char*)d->entries.Data+slot*sizeof(p),&p,sizeof(p));++d->entries.Count;}else ((xide_diagnostic_dependency**)d->entries.Data)[slot]=p;
  p->slot=slot;++d->stats.dictionaries;++d->stats.pending;fresh=NULL;
 } else if(stopped(cancelled,user)) {result=XIDE_DEPENDENCY_CANCELLED;goto done;}
 /* All fallible preparation is complete. Move the immutable leaves/keys in one
    commit; a rejected later part never damages an accepted prefix. */
 if(count) memcpy((unsigned char*)p->paths.Data+previous*sizeof(dependency_path),staged.Data,count*sizeof(dependency_path));
 p->paths.Count+=count;staged.Count=0;p->next++;p->encodedBytes+=bytes;d->stats.encodedBytes+=bytes;p->complete=complete;
 if(complete) --d->stats.pending;
 result=complete ? XIDE_DEPENDENCY_COMPLETE : XIDE_DEPENDENCY_PART;
done:
 for(size_t i=0;i<staged.Count;++i) {dependency_path* item=(dependency_path*)staged.Data+i;free(item->key);xrtValueRelease(item->value);}
 xrtArrayUnit(&staged);xideDiagnosticDependencyRelease(fresh);return result;
}
int xideDiagnosticDependenciesReference(xide_diagnostic_dependencies* d,const char* path,const char* id)
{
 if(!d) return 0;
 d->error[0]=0;
 xide_diagnostic_dependency* p=id ? find(d,id) : NULL;
 if((id && (!p || !p->complete)) || !path || !xrtPathIsAbs(path) || strlen(path)>65535) return fail(d,"Invalid dependency file binding.");
 wchar_t* key=xidePathNativeWide(path);if(!key) return fail(d,"Invalid dependency binding path.");
 if(p && !xideDiagnosticDependencyContainsKey(p,key)) {free(key);return fail(d,"Dependency set does not contain its diagnostic file.");}
 binding* list=(binding*)d->bindings.Data;size_t low=0,high=d->bindings.Count;
 while(low<high) {size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(list[mid].key,key)<0) low=mid+1;else high=mid;}
 int found=low<d->bindings.Count && !xidePathNativeKeyCompare(list[low].key,key);
 size_t bytes=(wcslen(key)+1)*sizeof(wchar_t);
 if(found) {
  free(key);xide_diagnostic_dependency* old=list[low].dependency;if(old==p) return 1;--old->bindings;
  if(p) {list[low].dependency=p;++p->bindings;}
  else {d->stats.bindingKeyBytes-=(wcslen(list[low].key)+1)*sizeof(wchar_t);free(list[low].key);memmove(list+low,list+low+1,(d->bindings.Count-low-1)*sizeof(binding));--d->bindings.Count;--d->stats.bindings;}
  return 1;
 }
 if(!p) {free(key);return 1;}
 if(d->stats.bindings==d->limits.bindings || bytes>d->limits.bindingKeyBytes-d->stats.bindingKeyBytes || !xrtArrayReserve(&d->bindings,d->bindings.Count+1)) {free(key);return fail(d,"Dependency binding capacity exceeded.");}
 list=(binding*)d->bindings.Data;memmove(list+low+1,list+low,(d->bindings.Count-low)*sizeof(binding));list[low]=(binding){key,p};++d->bindings.Count;++p->bindings;++d->stats.bindings;d->stats.bindingKeyBytes+=bytes;return 1;
}
