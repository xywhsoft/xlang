#include "protocol/workspace_folders.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
typedef struct folder {xvalue *uri,*name;char* path;wchar_t* key;size_t bytes;} folder;
struct xide_workspace_folders {xatomic32 references;size_t count,bytes;folder entries[XIDE_WORKSPACE_FOLDERS_LIMIT];};
static const xvalue* get(const xvalue* v,const char* k)
{return xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,xrtStrView(k)) : NULL;}
static int fail(xide_model_error* e,const char* message)
{if(e){e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s",message);}return 0;}
static const char* text(const xvalue* v,size_t limit)
{xstrview s={0};return xrtValueGetString(v,&s) && s.Size<=limit && (!s.Size || (s.Data && !memchr(s.Data,0,s.Size))) && xrtUtf8Valid(s,NULL) ? s.Data : NULL;}
static const char* label(const xvalue* v)
{xstrview s={0};xrtValueGetString(v,&s);return s.Data;}
static void clear(folder* f)
{xrtValueRelease(f->uri);xrtValueRelease(f->name);free(f->path);free(f->key);memset(f,0,sizeof(*f));}
static xide_workspace_folders* create(void)
{xide_workspace_folders* r=xrtCalloc(1,sizeof(*r));if(r){xrtAtomic32Store(&r->references,1,XMEMORY_RELAXED);r->bytes=sizeof(*r);}return r;}
void xideWorkspaceFoldersRelease(xide_workspace_folders* r)
{if(r && xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)==1){for(size_t i=0;i<r->count;++i)clear(r->entries+i);xrtFree(r);}}
xide_workspace_folders* xideWorkspaceFoldersRetain(xide_workspace_folders* r)
{if(r)xrtAtomic32FetchAdd(&r->references,1,XMEMORY_RELAXED);return r;}
size_t xideWorkspaceFoldersCount(const xide_workspace_folders* r){return r ? r->count : 0;}
int xideWorkspaceFoldersAt(const xide_workspace_folders* r,size_t i,const char** uri,const char** name,const char** path)
{if(!r || i>=r->count)return 0;const folder* f=r->entries+i;if(uri)*uri=label(f->uri);if(name)*name=label(f->name);if(path)*path=f->path;return 1;}
static size_t find(const xide_workspace_folders* r,const wchar_t* key)
{for(size_t i=0;i<r->count;++i)if(!xidePathNativeKeyCompare(r->entries[i].key,key))return i;return r->count;}
static int add(xide_workspace_folders* r,const xvalue* uri,const xvalue* name,int replace,xide_model_error* e)
{
 folder f={0};const char *u=text(uri,262144),*n=text(name,65535);
 if(!u || !*u || !n)return fail(e,"Workspace folder requires a file URI and a UTF-8 display name.");
 f.path=xideLspUriToPath(u,e);if(!f.path)return 0;
 if(strlen(f.path)>32768 || !(f.key=xidePathLexicalWide(f.path))){clear(&f);return fail(e,"Workspace folder path is invalid or too large.");}
 f.bytes=strlen(u)+strlen(n)+strlen(f.path)+3+(wcslen(f.key)+1)*sizeof(wchar_t);
 size_t i=find(r,f.key),old=i<r->count ? r->entries[i].bytes : 0;
 if((i<r->count && !replace) || (i==r->count && r->count==XIDE_WORKSPACE_FOLDERS_LIMIT) || f.bytes>XIDE_WORKSPACE_FOLDERS_BYTES-(r->bytes-old)){
  clear(&f);return fail(e,"Workspace folders contain duplicate paths or exceed metadata capacity.");
 }
 f.uri=xrtValueRetain((xvalue*)uri);f.name=xrtValueRetain((xvalue*)name);
 if(i<r->count)clear(r->entries+i);else ++r->count;
 r->entries[i]=f;r->bytes=r->bytes-old+f.bytes;return 1;
}
static xide_workspace_folders* readArray(const xvalue* array,xide_model_error* e)
{
 if(xrtValueType(array)!=XVALUE_ARRAY || xrtValueCount(array)>XIDE_WORKSPACE_FOLDERS_LIMIT){fail(e,"Workspace folder list must be an array within the root limit.");return NULL;}
 xide_workspace_folders* r=create();if(!r){fail(e,"Cannot allocate workspace folder metadata.");return NULL;}
 for(size_t i=0;i<xrtValueCount(array);++i){const xvalue* item=xrtValueArrayGet(array,i);if(!add(r,get(item,"uri"),get(item,"name"),0,e)){xideWorkspaceFoldersRelease(r);return NULL;}}
 return r;
}
xide_workspace_folders* xideWorkspaceFoldersRead(const xvalue* params,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(xrtValueType(params)!=XVALUE_OBJECT){fail(e,"Initialize parameters must be an object.");return NULL;}
 const xvalue *roots=get(params,"workspaceFolders"),*uri=get(params,"rootUri"),*path=get(params,"rootPath");
 if(roots && xrtValueType(roots)!=XVALUE_NULL)return readArray(roots,e);
 xide_workspace_folders* r=create();if(!r){fail(e,"Cannot allocate workspace folder metadata.");return NULL;}
 if(roots || (uri && xrtValueType(uri)==XVALUE_NULL) || (!uri && (!path || xrtValueType(path)==XVALUE_NULL)))return r;
 xvalue *owned=NULL,*name=xrtValueString(xrtStrView(""));
 if(!uri){const char* p=text(path,32768);char* u=p ? xideLspPathToUri(p,e) : NULL;owned=u ? xrtValueString(xrtStrView(u)) : NULL;free(u);uri=owned;}
 int okay=name && add(r,uri,name,0,e);xrtValueRelease(name);xrtValueRelease(owned);
 if(!okay){if(e && !*e->message)fail(e,"Invalid workspace root or allocation failure.");xideWorkspaceFoldersRelease(r);return NULL;}return r;
}
xide_workspace_folders* xideWorkspaceFoldersChange(const xide_workspace_folders* current,const xvalue* params,int* changed,xide_model_error* e)
{
 if(e)memset(e,0,sizeof(*e));
 if(changed)*changed=0;
 const xvalue* event=get(params,"event");
 xide_workspace_folders *removed=readArray(get(event,"removed"),e),*added=removed ? readArray(get(event,"added"),e) : NULL,*next=NULL;
 if(!removed || !added)goto done;
 if(current && !removed->count && !added->count){next=xideWorkspaceFoldersRetain((xide_workspace_folders*)current);goto done;}
 next=create();if(!next){fail(e,"Cannot allocate workspace folder metadata.");goto done;}
 for(size_t i=0;current && i<current->count;++i){const folder* f=current->entries+i;if(find(removed,f->key)==removed->count && !add(next,f->uri,f->name,0,e))goto failed;}
 for(size_t i=0;i<added->count;++i){const folder* f=added->entries+i;if(!add(next,f->uri,f->name,1,e))goto failed;}
 int difference=next->count!=xideWorkspaceFoldersCount(current);
 for(size_t i=0;current && !difference && i<next->count;++i)difference=find(current,next->entries[i].key)==current->count;
 if(changed)*changed=difference;
 goto done;
failed:xideWorkspaceFoldersRelease(next);next=NULL;
done:xideWorkspaceFoldersRelease(removed);xideWorkspaceFoldersRelease(added);return next;
}
