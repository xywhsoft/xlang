#include "services/workspace_discovery.h"
#ifdef XLS_COMPILER_XRT
#include "services/analysis_context.h"
#include "analysis/pool_idle.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#define DISCOVERY_LABELS (16u*1024u*1024u)
#define DISCOVERY_MANIFESTS 256u
typedef struct discovery_dir {char* path;wchar_t* key;xfileinfo identity;unsigned char hash[32];} discovery_dir;
typedef struct discovery_manifest {char* path;wchar_t* key;xfileinfo identity;unsigned char hash[32];int kind;} discovery_manifest;
typedef struct discovery_path {char* path;wchar_t* key;} discovery_path;
typedef struct discovery_name {char* name;size_t size;xfiletype type;} discovery_name;
typedef struct discovery_context {const wchar_t* key;xvalue* value;} discovery_context;
typedef struct discovery_proof {xatomic32 refs,invalid;xide_discovery_limits limits;xarray directories,manifests;} discovery_proof;
struct xide_discovery_snapshot {xatomic32 refs;discovery_proof* proof;xvalue* value;xide_analysis_catalog* catalog;unsigned char fingerprint[32];};
static void* proofRetain(void*);static void proofRelease(void*);
static int catalogProof(void*,int (*)(void*),void*,char*,size_t);
static size_t proofBytes(const discovery_proof*);
struct xide_workspace_discovery {xtaskpool* pool;xide_discovery_limits limits;};
struct xide_discovery_query {xfuture* future;xfuturewatch watch;int initialized,linked;void (*wake)(void*);void* user;};
typedef struct discovery_input {xide_workspace_folders* roots;char* package;xide_discovery_limits limits;} discovery_input;
typedef struct discovery_output {xide_discovery_snapshot* snapshot;xide_discovery_work work;char error[512];} discovery_output;
typedef struct discovery_build {discovery_input* input;discovery_output* output;xide_discovery_snapshot* snapshot;xarray paths,contexts;xmap directories;xvalue* projects;size_t contextBytes,bindings,labels;xcancel* cancel;} discovery_build;
static int stopped(void* p){return p && xrtCancelRequested(p);}
static int fail(xide_model_error* e,const char* reason)
{if(e){if(!e->code)e->code=XIDE_MODEL_IO;if(reason!=e->message)snprintf(e->message,sizeof(e->message),"%s",reason);}return 0;}
static char* copy(const char* s){size_t n=strlen(s)+1;char* p=malloc(n);if(p)memcpy(p,s,n);return p;}
static int charge(size_t* used,size_t add,size_t limit){if(*used>limit || add>limit-*used)return 0;*used+=add;return 1;}
static int put(xvalue* v,const char* name,xvalue* owned){return owned && xrtValueObjectSetNew(v,xrtStrView(name),owned);}
static int cancelled(int (*stop)(void*),void* p){return stop && stop(p);}
static int identity(const xfileinfo* a,const xfileinfo* b,xfiletype type)
{return a->Type==type && b->Type==type && (a->Available&XFILE_INFO_IDENTITY) && (b->Available&XFILE_INFO_IDENTITY) && a->Device==b->Device && a->Identity==b->Identity;}
static int sameFile(const xfileinfo* a,const xfileinfo* b)
{uint32 fields=XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_CHANGE_TIME;return identity(a,b,XFILE_TYPE_FILE) && (a->Available&XFILE_INFO_SIZE) &&
 (a->Available&fields)==(b->Available&fields) && a->Size==b->Size && (!(a->Available&XFILE_INFO_MODIFY_TIME)||a->Modified==b->Modified) && (!(a->Available&XFILE_INFO_CHANGE_TIME)||a->Changed==b->Changed);}
static int ignored(xstrview name)
{
 static const char* const names[]={".git",".hg",".svn","extlibs","node_modules","build","release","release-materials",".xlang-ide",".xlang-update","update_info",".cache"};
 for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(name.Size==strlen(names[i])&&!_strnicmp(name.Data,names[i],name.Size))return 1;
 return 0;
}
static int sourceName(xstrview n)
{return (n.Size>=3&&!_strnicmp(n.Data+n.Size-3,".xl",3)) || (n.Size>=4&&(!_strnicmp(n.Data+n.Size-4,".xlp",4)||!_strnicmp(n.Data+n.Size-4,".xlw",4)));}
int xideWorkspaceDiscoveryRelevantPath(const xide_workspace_folders* roots,const char* path)
{
 wchar_t* key=xidePathLexicalWide(path);if(!key)return 1;size_t length=wcslen(key);int matched=0;
 for(size_t i=0;i<xideWorkspaceFoldersCount(roots);++i){const char* root=NULL;
  if(!xideWorkspaceFoldersAt(roots,i,NULL,NULL,&root)){free(key);return 1;}
  wchar_t* base=xidePathLexicalWide(root);if(!base){free(key);return 1;}size_t n=wcslen(base);
  int within=length>=n && CompareStringOrdinal(key,(int)n,base,(int)n,TRUE)==CSTR_EQUAL && (length==n || base[n-1]==L'/' || key[n]==L'/');free(base);if(!within)continue;
  matched=1;int excluded=0;const wchar_t* at=key+n;if(*at==L'/')++at;
  while(*at){const wchar_t* end=wcschr(at,L'/');if(!end)end=at+wcslen(at);size_t size=(size_t)(end-at);char ascii[32];int plain=size<sizeof(ascii);
   for(size_t j=0;plain && j<size;++j){plain=at[j]<128;if(plain)ascii[j]=(char)at[j];}
   if(plain && ignored((xstrview){ascii,size})){excluded=1;break;}at=*end?end+1:end;
  }
  if(!excluded){free(key);return 1;}
 }
 free(key);return !matched;
}
static void namesClear(xarray* a)
{for(size_t i=0;i<a->Count;++i)xrtFree(((discovery_name*)a->Data)[i].name);xrtArrayUnit(a);}
static int nameOrder(const void* a,const void* b){return strcmp(((const discovery_name*)a)->name,((const discovery_name*)b)->name);}
static int pathOrder(const void* a,const void* b)
{const discovery_path *x=a,*y=b;int key=xidePathNativeKeyCompare(x->key,y->key);return key ? key : strcmp(x->path,y->path);}
static int contextOrder(const void* a,const void* b)
{return xidePathNativeKeyCompare(((const discovery_context*)a)->key,((const discovery_context*)b)->key);}
static int hashNumber(xsha256* hash,uint64_t value)
{unsigned char bytes[8];for(unsigned i=0;i<8;++i)bytes[i]=(unsigned char)(value>>(i*8));return xrtSha256Update(hash,bytes,8);}
/* Exact relevant names,not directory mtime heuristics. No source-body reads. */
static int enumerate(const char* path,xarray* names,unsigned char hash[32],const xide_discovery_limits* limits,size_t* visited,
 int (*stop)(void*),void* user,xide_model_error* e)
{
 if(!xrtArrayInit(names,sizeof(discovery_name)))return fail(e,"Cannot allocate directory enumeration.");
 char* native=xidePathNativeUtf8(path);xdir dir=native ? xrtDirOpen(native,0) : NULL;free(native);
 if(!dir)return fail(e,"Cannot enumerate a workspace directory.");
 int okay=1;size_t bytes=0;xdirentry entry; xdirnext state=XDIR_NEXT_END;
 while(!cancelled(stop,user) && (state=xrtDirNext(dir,&entry))==XDIR_NEXT_ITEM){
  xstrview n=entry.Name;
  if((n.Size==1&&n.Data[0]=='.') || (n.Size==2&&!memcmp(n.Data,"..",2)))continue;
  if(!charge(visited,1,limits->entries)){okay=fail(e,"Workspace discovery entry limit exceeded.");break;}
  if(!(entry.Flags&XDIR_ENTRY_UTF8) || !n.Size || n.Size>32767 || memchr(n.Data,0,n.Size) || memchr(n.Data,'/',n.Size) || memchr(n.Data,'\\',n.Size) || !xrtUtf8Valid(n,NULL)){okay=fail(e,"Workspace directory has an unsupported name.");break;}
  for(size_t i=0;i<n.Size;++i)if((unsigned char)n.Data[i]<32){okay=fail(e,"Workspace name cannot be represented by the file URI protocol.");break;}
  if(!okay)break;
  xfiletype type=entry.Info.Type;
  if((type==XFILE_TYPE_DIRECTORY || type==XFILE_TYPE_LINK) && ignored(n))continue;
  if(type==XFILE_TYPE_NONE){okay=fail(e,"Workspace entry type is unavailable.");break;}
  if(type==XFILE_TYPE_LINK){okay=fail(e,"Workspace discovery cannot silently traverse an unexcluded link.");break;}
  if(type!=XFILE_TYPE_DIRECTORY && !sourceName(n))continue;
  if(type!=XFILE_TYPE_DIRECTORY && type!=XFILE_TYPE_FILE){okay=fail(e,"Workspace source or manifest is not a regular file.");break;}
  if(!charge(&bytes,n.Size+1+sizeof(discovery_name),limits->labels)){okay=fail(e,"Workspace directory name storage exceeded its limit.");break;}
  discovery_name item={.size=n.Size,.type=type};item.name=xrtMalloc(n.Size+1);
  if(item.name){memcpy(item.name,n.Data,n.Size);item.name[n.Size]=0;}
  if(!item.name || !xrtArrayPush(names,&item)){xrtFree(item.name);okay=fail(e,"Cannot retain workspace directory names.");break;}
 }
 if(cancelled(stop,user))okay=0;
 else if(okay && state!=XDIR_NEXT_END)okay=fail(e,"Workspace directory enumeration failed.");
 if(!xrtDirClose(dir))okay=fail(e,"Cannot close workspace directory enumeration.");
 if(!okay)return 0;
 if(names->Count>1)qsort(names->Data,names->Count,sizeof(discovery_name),nameOrder);
 xsha256 sha;xrtSha256Init(&sha);static const char domain[]="xlang/workspace-directory-v1";
 okay=xrtSha256Update(&sha,domain,sizeof(domain))&&hashNumber(&sha,names->Count);
 for(size_t i=0;i<names->Count && okay;++i){discovery_name* n=(discovery_name*)names->Data+i;
  if(i && !strcmp(n[-1].name,n->name))return fail(e,"Workspace names changed while enumerating.");
  okay=hashNumber(&sha,(uint64_t)n->type)&&hashNumber(&sha,n->size)&&xrtSha256Update(&sha,n->name,n->size);
 }
 return (okay && xrtSha256Final(&sha,hash)) || fail(e,"Cannot fingerprint workspace directory names.");
}
static int addDirectory(discovery_build* b,const char* path,int root,xide_model_error* e)
{
 xfileinfo info={0};if(!xideIoStat(path,root,&info) || info.Type!=XFILE_TYPE_DIRECTORY || !(info.Available&XFILE_INFO_IDENTITY))return fail(e,"Workspace root or directory is missing,linked or has no stable identity.");
 uint64_t key[2]={info.Device,info.Identity};bool added=false;size_t* index=xrtMapGetOrAdd(&b->directories,(xbytesview){(const unsigned char*)key,sizeof(key)},&added);
 if(!index)return fail(e,"Cannot index workspace directory identities.");
 if(!added){discovery_dir* prior=(discovery_dir*)b->snapshot->proof->directories.Data+*index;return xidePathLexicalEqual(prior->path,path) || fail(e,"Workspace directory aliases need explicit configuration.");}
 if(b->snapshot->proof->directories.Count==b->input->limits.directories)return fail(e,"Workspace discovery directory limit exceeded.");
 discovery_dir dir={.identity=info};dir.path=copy(path);dir.key=xidePathLexicalWide(path);
 if(!dir.path || !dir.key || strlen(path)>32767 || !charge(&b->labels,strlen(path)+1+(wcslen(dir.key)+1)*sizeof(wchar_t)+sizeof(dir),b->input->limits.labels) || !xrtArrayPush(&b->snapshot->proof->directories,&dir)){
  free(dir.path);free(dir.key);return fail(e,"Workspace directory metadata exceeded its allocation or byte limit.");
 }
 *index=b->snapshot->proof->directories.Count-1;return 1;
}
static int addPath(discovery_build* b,const char* path,xide_model_error* e)
{
 discovery_path item={.path=copy(path),.key=xidePathLexicalWide(path)};
 if(b->paths.Count==65536 || !item.path || !item.key || strlen(path)>32767 || !charge(&b->labels,strlen(path)+1+(wcslen(item.key)+1)*sizeof(wchar_t)+sizeof(item),b->input->limits.labels) || !xrtArrayPush(&b->paths,&item)){
  free(item.path);free(item.key);return fail(e,"Workspace member metadata exceeded its allocation or byte limit.");
 }return 1;
}
static int scan(discovery_build* b,xide_model_error* e)
{
 for(size_t i=0;i<xideWorkspaceFoldersCount(b->input->roots);++i){const char* path=NULL;if(stopped(b->cancel) || !xideWorkspaceFoldersAt(b->input->roots,i,NULL,NULL,&path) || !addDirectory(b,path,1,e))return 0;}
 for(size_t i=0;i<b->snapshot->proof->directories.Count;++i){
  if(stopped(b->cancel))return 0;
  /* AddDirectory can reallocate the vector;retain only this stable path. */
  const char* path=((discovery_dir*)b->snapshot->proof->directories.Data)[i].path;xarray names={0};unsigned char hash[32];xfileinfo before={0},after={0};
  int okay=xideIoStat(path,1,&before)&&identity(&before,&((discovery_dir*)b->snapshot->proof->directories.Data)[i].identity,XFILE_TYPE_DIRECTORY) &&
   enumerate(path,&names,hash,&b->input->limits,&b->output->work.entries,stopped,b->cancel,e);
  for(size_t j=0;okay&&j<names.Count;++j){discovery_name* name=(discovery_name*)names.Data+j;char* child=xidePathResolveLexical(path,name->name);
   okay=child && (name->type==XFILE_TYPE_DIRECTORY ? addDirectory(b,child,0,e) : addPath(b,child,e));free(child);
  }
  if(okay)okay=xideIoStat(path,1,&after)&&identity(&before,&after,XFILE_TYPE_DIRECTORY);
  if(okay)memcpy(((discovery_dir*)b->snapshot->proof->directories.Data)[i].hash,hash,32);
  namesClear(&names);if(!okay)return fail(e,*e->message ? e->message : "Workspace directory changed during discovery.");
 }
 b->output->work.directories=b->snapshot->proof->directories.Count;
 if(b->paths.Count>1)qsort(b->paths.Data,b->paths.Count,sizeof(discovery_path),pathOrder);
 return 1;
}
static int readManifest(const char* path,const xide_discovery_limits* limits,size_t* bytes,xfileinfo* identityOut,unsigned char hash[32],xvalue** decoded,
 int (*stop)(void*),void* user,xide_model_error* e)
{
 if(decoded)*decoded=NULL;
 xfile file=xideIoOpen(path,XFILE_READ|XFILE_NOFOLLOW);xfileinfo before={0},after={0};unsigned char* raw=NULL;int okay=0;
 if(!file || !xideIoReceiptFileStat(file,&before) || before.Type!=XFILE_TYPE_FILE || !(before.Available&XFILE_INFO_SIZE) || before.Size>limits->manifestBytes || !charge(bytes,(size_t)before.Size,limits->manifestBytes))goto done;
 raw=xrtMalloc((size_t)before.Size+1);if(!raw)goto done;
 for(size_t at=0;at<(size_t)before.Size;){size_t part=(size_t)before.Size-at;if(part>65536)part=65536;size_t got=0;if(cancelled(stop,user)||!xrtReadFull(file,raw+at,part,&got)||got!=part)goto done;at+=part;}
 raw[(size_t)before.Size]=0;
 if(!xideIoReceiptFileStat(file,&after) || !sameFile(&before,&after) || !xrtSha256(raw,(size_t)before.Size,hash))goto done;
 *identityOut=before;
 if(decoded){size_t bom=before.Size>=3&&raw[0]==0xef&&raw[1]==0xbb&&raw[2]==0xbf ? 3 : 0;*decoded=xrtJsonParse((xstrview){(const char*)raw+bom,(size_t)before.Size-bom});if(xrtValueType(*decoded)!=XVALUE_OBJECT)goto done;}
 okay=1;
done:
 xrtFree(raw);if(file && !xrtClose(file))okay=0;
 if(!okay){if(decoded){xrtValueRelease(*decoded);*decoded=NULL;}if(!cancelled(stop,user))fail(e,"Workspace manifest is missing,invalid,unstable or exceeds its byte limit.");}
 return okay;
}
/* Each referenced manifest is decoded from the exact guarded raw bytes;never
 * call model Load and then hash a second,different read as its provenance. */
static int loadManifest(discovery_build* b,const char* path,int kind,xide_model_error* e)
{
 if(stopped(b->cancel))return 0;
 wchar_t* key=xidePathLexicalWide(path);if(!key)return fail(e,"Cannot normalize a workspace manifest.");
 for(size_t i=0;i<b->snapshot->proof->manifests.Count;++i){discovery_manifest* old=(discovery_manifest*)b->snapshot->proof->manifests.Data+i;if(!xidePathNativeKeyCompare(key,old->key)){
   free(key);xfileinfo info={0};if(strcmp(path,old->path) && (!xideIoReceiptStat(path,&info)||!identity(&info,&old->identity,XFILE_TYPE_FILE)))return fail(e,"Manifest paths collide under native lexical comparison.");
   return old->kind==kind || fail(e,"A manifest has conflicting workspace/project roles.");
  }}
 if(b->snapshot->proof->manifests.Count==DISCOVERY_MANIFESTS){free(key);return fail(e,"Workspace manifest count exceeded its limit.");}
 discovery_manifest record={.key=key,.path=copy(path),.kind=kind};xvalue* root=NULL;int okay=record.path && charge(&b->labels,strlen(path)+1+(wcslen(key)+1)*sizeof(wchar_t)+sizeof(record),b->input->limits.labels) &&
  readManifest(path,&b->input->limits,&b->output->work.manifestBytes,&record.identity,record.hash,&root,stopped,b->cancel,e);
 if(okay)okay=xrtArrayPush(&b->snapshot->proof->manifests,&record);
 if(!okay){free(record.path);free(key);xrtValueRelease(root);return fail(e,*e->message ? e->message : "Cannot retain workspace manifest provenance.");}
 xide_model_decode_options options={.paths=65536,.projects=64,.labels=b->input->limits.labels,.cancelled=stopped,.user=b->cancel};
 if(kind==1){xide_workspace model={0};okay=xideWorkspaceDecodeWithOptions(&model,path,root,&options,e);
  for(size_t i=0;okay&&i<model.projectCount;++i)okay=loadManifest(b,model.projects[i].manifest,2,e);
  for(size_t i=0;okay&&i<model.files.count;++i)if(xidePathHasExtension(model.files.items[i].path,".xl"))okay=addPath(b,model.files.items[i].path,e);
  xideWorkspaceClear(&model);
 }else{xide_project model={0};okay=xideProjectDecodeWithOptions(&model,path,root,&options,e);
  if(okay && model.entry && xidePathHasExtension(model.entry,".xl")){
   xls_context* context=xideAnalysisProjectContext(&model,NULL,XIDE_ANALYSIS_PROFILE,b->input->package,e);
   okay=context && b->contexts.Count<64 && charge(&b->contextBytes,xlsContextBytes(context),16u*1024u*1024u) &&
    charge(&b->bindings,1+model.files.count,65536);
   xvalue* wire=okay ? xideAnalysisContextWrite(context) : NULL;discovery_context role={record.key,wire};okay=wire&&xrtArrayPush(&b->contexts,&role);if(!okay)xrtValueRelease(wire);xlsContextRelease(context);
  }
  xideProjectClear(&model);
 }
 xrtValueRelease(root);return okay || fail(e,*e->message ? e->message : "Workspace project decoding or scope capacity failed.");
}
static int prepareCatalog(discovery_build* b,xide_model_error* e)
{
 /* Workspaces may reference projects outside roots. Subsequent enumerated
    duplicates are skipped by the same prepared native manifest key. */
 size_t initial=b->paths.Count;
 for(int kind=1;kind<=2;++kind)for(size_t i=0;i<initial;++i){const char* path=((discovery_path*)b->paths.Data)[i].path;
  if(xidePathHasExtension(path,kind==1?".xlw":".xlp")&&!loadManifest(b,path,kind,e))return 0;
 }
 if(b->paths.Count>1)qsort(b->paths.Data,b->paths.Count,sizeof(discovery_path),pathOrder);
 xvalue *value=xrtValueObject(),*roots=xrtValueArray();int okay=value&&roots;
 if(b->contexts.Count>1)qsort(b->contexts.Data,b->contexts.Count,sizeof(discovery_context),contextOrder);
 for(size_t i=0;okay&&i<b->contexts.Count;++i)okay=xrtValueArrayAppend(b->projects,((discovery_context*)b->contexts.Data)[i].value);
 for(size_t i=0;okay&&i<b->paths.Count;++i){discovery_path* row=(discovery_path*)b->paths.Data+i;if(!xidePathHasExtension(row->path,".xl"))continue;
  if(i && !xidePathNativeKeyCompare(row[-1].key,row->key)){
   xfileinfo a={0},c={0};if(strcmp(row[-1].path,row->path) && (!xideIoStat(row[-1].path,1,&a)||!xideIoStat(row->path,1,&c)||!identity(&a,&c,XFILE_TYPE_FILE))){okay=fail(e,"Source paths collide under native lexical comparison.");break;}continue;
  }
  xvalue* path=xrtValueString(xrtStrView(row->path));okay=path&&xrtValueArrayAppend(roots,path);xrtValueRelease(path);++b->output->work.sources;
 }
 if(okay)okay=put(value,"schema",xrtValueInt(2))&&put(value,"selectedProject",xrtValueString(xrtStrView("")))&&put(value,"packagePath",b->input->package?xrtValueString(xrtStrView(b->input->package)):xrtValueNull())&&put(value,"complete",xrtValueBool(true));
 if(okay){okay=put(value,"projects",b->projects);b->projects=NULL;}
 if(okay){okay=put(value,"standaloneRoots",roots);roots=NULL;}
 if(okay){size_t bytes=proofBytes(b->snapshot->proof);xide_analysis_catalog_guard guard={b->snapshot->proof,proofRetain,proofRelease,catalogProof,bytes};
  if(bytes)b->snapshot->catalog=xideAnalysisCatalogCreateGuarded(value,&guard,e->message,sizeof(e->message));}
 if(okay)okay=b->snapshot->catalog!=NULL;
 size_t size=0;char* encoded=okay?xrtJsonStringify(value,false,&size):NULL;
 if(okay)okay=encoded && size<=16u*1024u*1024u && xrtSha256(encoded,size,b->snapshot->fingerprint);
 xrtFree(encoded);xrtValueRelease(roots);
 if(okay){b->snapshot->value=value;b->output->work.projects=xideAnalysisCatalogCount(b->snapshot->catalog);}else xrtValueRelease(value);
 return okay || fail(e,*e->message ? e->message : "Workspace catalog allocation or capacity failed.");
}
xide_discovery_snapshot* xideDiscoverySnapshotRetain(xide_discovery_snapshot* s){if(s)xrtAtomic32FetchAdd(&s->refs,1,XMEMORY_RELAXED);return s;}
static void* proofRetain(void* raw)
{discovery_proof* s=raw;if(s)xrtAtomic32FetchAdd(&s->refs,1,XMEMORY_RELAXED);return s;}
static void proofRelease(void* raw)
{
 discovery_proof* s=raw;if(!s || xrtAtomic32FetchSub(&s->refs,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;i<s->directories.Count;++i){discovery_dir* d=(discovery_dir*)s->directories.Data+i;free(d->path);free(d->key);}
 for(size_t i=0;i<s->manifests.Count;++i){discovery_manifest* m=(discovery_manifest*)s->manifests.Data+i;free(m->path);free(m->key);}
 xrtArrayUnit(&s->directories);xrtArrayUnit(&s->manifests);xrtFree(s);
}
static size_t proofBytes(const discovery_proof* s)
{
 size_t bytes=sizeof(*s);
 if(s->directories.Capacity>SIZE_MAX/sizeof(discovery_dir) || s->manifests.Capacity>SIZE_MAX/sizeof(discovery_manifest) ||
  !charge(&bytes,s->directories.Capacity*sizeof(discovery_dir),SIZE_MAX) || !charge(&bytes,s->manifests.Capacity*sizeof(discovery_manifest),SIZE_MAX))return 0;
 for(size_t i=0;i<s->directories.Count;++i){discovery_dir* d=(discovery_dir*)s->directories.Data+i;
  if(!charge(&bytes,strlen(d->path)+1,SIZE_MAX) || !charge(&bytes,(wcslen(d->key)+1)*sizeof(wchar_t),SIZE_MAX))return 0;}
 for(size_t i=0;i<s->manifests.Count;++i){discovery_manifest* m=(discovery_manifest*)s->manifests.Data+i;
  if(!charge(&bytes,strlen(m->path)+1,SIZE_MAX) || !charge(&bytes,(wcslen(m->key)+1)*sizeof(wchar_t),SIZE_MAX))return 0;}
 return bytes;
}
void xideDiscoverySnapshotRelease(xide_discovery_snapshot* s)
{
 if(!s || xrtAtomic32FetchSub(&s->refs,1,XMEMORY_ACQ_REL)!=1)return;
 xrtValueRelease(s->value);xideAnalysisCatalogDestroy(s->catalog);proofRelease(s->proof);xrtFree(s);
}
xide_analysis_catalog* xideDiscoverySnapshotCatalog(const xide_discovery_snapshot* s){return s?s->catalog:NULL;}
const xvalue* xideDiscoverySnapshotValue(const xide_discovery_snapshot* s){return s?s->value:NULL;}
const unsigned char* xideDiscoverySnapshotFingerprint(const xide_discovery_snapshot* s){return s?s->fingerprint:NULL;}
int xideDiscoverySnapshotInvalidated(const xide_discovery_snapshot* s)
{return s && xrtAtomic32Load(&s->proof->invalid,XMEMORY_ACQUIRE)!=0;}
static int proofValidate(discovery_proof* s,const xide_discovery_limits* limits,int (*stop)(void*),void* user,xide_model_error* e)
{
 if(xrtAtomic32Load(&s->invalid,XMEMORY_ACQUIRE)){fail(e,"Workspace discovery snapshot was already invalidated.");return XIDE_DISCOVERY_STALE;}
 size_t entries=0,bytes=0;
 for(size_t i=0;i<s->directories.Count;++i){if(cancelled(stop,user))return XIDE_DISCOVERY_CANCELLED;discovery_dir* d=(discovery_dir*)s->directories.Data+i;xfileinfo before={0},after={0};xarray names={0};unsigned char hash[32];
  int okay=xideIoStat(d->path,1,&before)&&identity(&before,&d->identity,XFILE_TYPE_DIRECTORY)&&enumerate(d->path,&names,hash,limits,&entries,stop,user,e)&&
   xideIoStat(d->path,1,&after)&&identity(&after,&d->identity,XFILE_TYPE_DIRECTORY)&&!memcmp(hash,d->hash,32);namesClear(&names);
  if(!okay){if(cancelled(stop,user))return XIDE_DISCOVERY_CANCELLED;goto stale;}
 }
 for(size_t i=0;i<s->manifests.Count;++i){if(cancelled(stop,user))return XIDE_DISCOVERY_CANCELLED;discovery_manifest* m=(discovery_manifest*)s->manifests.Data+i;xfileinfo info={0};unsigned char hash[32];
  if(!readManifest(m->path,limits,&bytes,&info,hash,NULL,stop,user,e)||!sameFile(&info,&m->identity)||memcmp(hash,m->hash,32)){if(cancelled(stop,user))return XIDE_DISCOVERY_CANCELLED;goto stale;}
 }
 return XIDE_DISCOVERY_READY;
stale:xrtAtomic32Store(&s->invalid,1,XMEMORY_RELEASE);fail(e,*e->message ? e->message : "Workspace membership or manifest changed.");return XIDE_DISCOVERY_STALE;
}
static int catalogProof(void* raw,int (*stop)(void*),void* user,char* error,size_t capacity)
{
 discovery_proof* s=raw;xide_model_error e={0};int okay=proofValidate(s,&s->limits,stop,user,&e)==XIDE_DISCOVERY_READY;
 if(!okay&&error&&capacity)snprintf(error,capacity,"%s",*e.message?e.message:"Workspace discovery evidence validation cancelled.");
 return okay;
}
int xideDiscoverySnapshotValidate(xide_discovery_snapshot* s,int (*stop)(void*),void* user,xide_model_error* e)
{xide_model_error local={0};if(!e)e=&local;memset(e,0,sizeof(*e));if(!s){fail(e,"No discovery snapshot.");return XIDE_DISCOVERY_FAILED;}return proofValidate(s->proof,&s->proof->limits,stop,user,e);}
static void outputFree(void* p,void* unused){(void)unused;discovery_output* out=p;if(out){xideDiscoverySnapshotRelease(out->snapshot);xrtFree(out);}}
static void inputFree(void* p,void* unused){(void)unused;discovery_input* in=p;if(in){xideWorkspaceFoldersRelease(in->roots);xrtFree(in->package);xrtFree(in);}}
static xtaskoutcome execute(xcancel* cancel,void* raw,xtaskvalue* value)
{
 discovery_input* in=raw;discovery_output* out=xrtCalloc(1,sizeof(*out));if(!out)return XTASK_FAILED;
 out->work.workerThread=xrtThreadCurrentId();xide_discovery_snapshot* s=xrtCalloc(1,sizeof(*s));xide_model_error e={0};discovery_build b={.input=in,.output=out,.snapshot=s,.cancel=cancel};
 if(s){xrtAtomic32Init(&s->refs,1);s->proof=xrtCalloc(1,sizeof(*s->proof));if(s->proof){xrtAtomic32Init(&s->proof->refs,1);xrtAtomic32Init(&s->proof->invalid,0);s->proof->limits=in->limits;}}
 int okay=s && s->proof && xrtArrayInit(&s->proof->directories,sizeof(discovery_dir)) && xrtArrayInit(&s->proof->manifests,sizeof(discovery_manifest)) && xrtArrayInit(&b.paths,sizeof(discovery_path)) && xrtArrayInit(&b.contexts,sizeof(discovery_context)) && xrtMapInit(&b.directories,sizeof(size_t)) && (b.projects=xrtValueArray())!=NULL;
 if(okay)okay=scan(&b,&e)&&prepareCatalog(&b,&e)&&proofValidate(s->proof,&in->limits,stopped,cancel,&e)==XIDE_DISCOVERY_READY;
 for(size_t i=0;i<b.paths.Count;++i){discovery_path* path=(discovery_path*)b.paths.Data+i;free(path->path);free(path->key);}xrtArrayUnit(&b.paths);
 for(size_t i=0;i<b.contexts.Count;++i)xrtValueRelease(((discovery_context*)b.contexts.Data)[i].value);
 xrtArrayUnit(&b.contexts);xrtMapUnit(&b.directories);xrtValueRelease(b.projects);
 if(stopped(cancel)){xideDiscoverySnapshotRelease(s);outputFree(out,NULL);return XTASK_CANCELLED;}
 if(okay){out->snapshot=s;out->work.manifests=s->proof->manifests.Count;out->work.labelBytes=b.labels;}else{xideDiscoverySnapshotRelease(s);snprintf(out->error,sizeof(out->error),"%s",*e.message?e.message:"Workspace discovery allocation failed.");}
 *value=(xtaskvalue){out,outputFree,NULL};return XTASK_SUCCESS;
}
xide_workspace_discovery* xideWorkspaceDiscoveryCreate(const xide_discovery_limits* requested,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 xide_discovery_limits limits={4096,131072,DISCOVERY_LABELS,DISCOVERY_LABELS};
 if(requested){size_t chosen[4]={limits.directories,limits.entries,limits.labels,limits.manifestBytes},input[4]={requested->directories,requested->entries,requested->labels,requested->manifestBytes};
  for(size_t i=0;i<4;++i)if(input[i]){if(input[i]>chosen[i]){fail(e,"Workspace discovery limits exceed supported maxima.");return NULL;}chosen[i]=input[i];}
  limits=(xide_discovery_limits){chosen[0],chosen[1],chosen[2],chosen[3]};
 }
 xide_workspace_discovery* store=xrtCalloc(1,sizeof(*store));if(!store)return NULL;store->limits=limits;xtaskpoolconfig config={.Threads=1,.QueueLimit=1};store->pool=xrtTaskPoolCreate(&config);if(!store->pool){xrtFree(store);return NULL;}return store;
}
void xideWorkspaceDiscoveryDestroy(xide_workspace_discovery* s){if(s){xrtTaskPoolCancel(s->pool);xrtTaskPoolDestroy(s->pool);xrtFree(s);}}
int xideWorkspaceDiscoveryIdle(xide_workspace_discovery* s){return !s || xlsRuntimeTaskPoolIdle(s->pool);}
xide_discovery_query* xideWorkspaceDiscoverySubmit(xide_workspace_discovery* s,xide_workspace_folders* roots,const char* package)
{
 if(!s || !roots || (package && (strlen(package)>262144 || !xrtUtf8Valid(xrtStrView(package),NULL))))return NULL;
 xide_discovery_query* q=xrtCalloc(1,sizeof(*q));discovery_input* in=q?xrtCalloc(1,sizeof(*in)):NULL;
 if(in){in->roots=xideWorkspaceFoldersRetain(roots);in->package=package?xrtStrDup(package):NULL;in->limits=s->limits;xtaskargs args={.Destroy=inputFree};if(!package || in->package)q->future=xrtTaskSubmit(s->pool,execute,in,&args);}
 if(!in || !q->future){inputFree(in,NULL);xrtFree(q);return NULL;}return q;
}
int xideWorkspaceDiscoveryResult(xide_discovery_query* q,const xide_discovery_snapshot** result,xide_discovery_work* work,char* error,size_t capacity)
{
 if(result)*result=NULL;
 if(work)*work=(xide_discovery_work){0};
 if(error&&capacity)*error=0;
 if(!q)return XIDE_DISCOVERY_FAILED;
 xfuturestate state=xrtFutureState(q->future);if(state==XFUTURE_PENDING)return XIDE_DISCOVERY_PENDING;if(state==XFUTURE_CANCELLED)return XIDE_DISCOVERY_CANCELLED;
 const discovery_output* out=state==XFUTURE_RESOLVED?xrtFutureValue(q->future):NULL;if(out&&work)*work=out->work;
 if(!out || !out->snapshot){if(error&&capacity)snprintf(error,capacity,"%s",out&&*out->error?out->error:"Workspace discovery task failed.");return XIDE_DISCOVERY_FAILED;}if(result)*result=out->snapshot;return XIDE_DISCOVERY_READY;
}
xide_discovery_snapshot* xideWorkspaceDiscoveryTake(xide_discovery_query* q)
{if(xideWorkspaceDiscoveryResult(q,NULL,NULL,NULL,0)!=XIDE_DISCOVERY_READY)return NULL;discovery_output* out=xrtFutureValue(q->future);xide_discovery_snapshot* s=out->snapshot;out->snapshot=NULL;return s;}
static void wakeQuery(void* p){xide_discovery_query* q=p;q->wake(q->user);}
int xideWorkspaceDiscoverySetWake(xide_discovery_query* q,void (*wake)(void*),void* user)
{if(!q||!wake||q->initialized)return 0;q->wake=wake;q->user=user;if(!xrtFutureWatchInit(&q->watch,wakeQuery,NULL,q))return 0;q->initialized=1;xfuturewatchresult state=xrtFutureWatchAdd(q->future,&q->watch);if(state==XFUTURE_WATCH_ERROR)return 0;q->linked=state==XFUTURE_WATCH_PENDING;if(state==XFUTURE_WATCH_READY)wake(user);return 1;}
void xideWorkspaceDiscoveryCancel(xide_discovery_query* q){if(q)xrtFutureCancel(q->future);}
void xideWorkspaceDiscoveryQueryDestroy(xide_discovery_query* q){if(q){if(q->linked)xrtFutureWatchRemove(q->future,&q->watch);xrtFutureDestroy(q->future);xrtFree(q);}}
#endif
