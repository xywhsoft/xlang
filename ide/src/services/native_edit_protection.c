#include "platform/runtime_library.h"
#include "services/native_edit_protection.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct protected_row {char* path;wchar_t* key;int directory;} protected_row;
struct xide_native_protection {protected_row rows[255];xide_edit_alias_exclusion paths[255];size_t count,text,bytes;};
static int fail(xide_model_error* e,int code,const char* text)
{if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static int bounded(const char* s)
{if(!s)return 0;size_t n=0;while(n<65536&&s[n])++n;return n&&n<65536;}
static int order(const void* a,const void* b)
{return xidePathNativeKeyCompare(((const protected_row*)a)->key,((const protected_row*)b)->key);}
static int add(xide_native_protection* p,const char* base,const char* path,int directory,xide_model_error* e)
{
 if(!path)return 1;
 if(!bounded(path)||(base&&!bounded(base)))return fail(e,XIDE_MODEL_INVALID,"Protected host paths must be nonempty bounded UTF8 paths.");
 char* absolute=xidePathResolveLexical(base,path);wchar_t* key=absolute?xidePathNativeWide(absolute):NULL;
 if(!absolute||!key){free(absolute);free(key);return fail(e,XIDE_MODEL_INVALID,"Cannot prepare a lexical protected host path.");}
 for(size_t i=0;i<p->count;++i)if(!xidePathNativeKeyCompare(key,p->rows[i].key)){
  if(p->rows[i].directory!=directory){free(absolute);free(key);return fail(e,XIDE_MODEL_CONFLICT,"One host metadata path has conflicting file/directory roles.");}
  free(absolute);free(key);return 1;
 }
 size_t text=strlen(absolute)+1,bytes=text+(wcslen(key)+1)*sizeof(wchar_t);
 if(p->count==255||text>1024u*1024u-p->text||bytes>4u*1024u*1024u-p->bytes){free(absolute);free(key);return fail(e,XIDE_MODEL_MEMORY,"Complete protected host policy exceeds transaction capacity.");}
 p->rows[p->count++]=(protected_row){absolute,key,directory};p->text+=text;p->bytes+=bytes;return 1;
}
static int runtime(xide_native_protection* p,const char* directory,xide_model_error* e)
{
 static const char* dirs[]={"data","extlibs","include","notices","option"};
 static const char* files[]={"xlang_ide.exe","xlang-ls.exe","xl.exe","xl.dll","xlc.dll","xge.dll",XIDE_RUNTIME_DLL_BASENAME,"update.exe","mdo.exe"};
 if(!bounded(directory))return fail(e,XIDE_MODEL_INVALID,"Missing host runtime directory.");
 for(size_t i=0;i<sizeof(dirs)/sizeof(*dirs);++i)if(!add(p,directory,dirs[i],1,e))return 0;
 for(size_t i=0;i<sizeof(files)/sizeof(*files);++i)if(!add(p,directory,files[i],0,e))return 0;
 return 1;
}
void xideNativeProtectionRelease(xide_native_protection* p)
{if(p){for(size_t i=0;i<p->count;++i){free(p->rows[i].path);free(p->rows[i].key);}xrtFree(p);}}
xide_native_protection* xideNativeProtectionCreate(const xide_native_protection_seed* seed,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!seed||!bounded(seed->runtimeDirectory)||!bounded(seed->compiler)||!bounded(seed->storageDirectory)||seed->projects>255||(seed->projects&&!seed->manifests))return(fail(e,XIDE_MODEL_INVALID,"Missing or excessive complete native host protection metadata."),NULL);
 xide_native_protection* p=xrtCalloc(1,sizeof(*p));if(!p)return(fail(e,XIDE_MODEL_MEMORY,"Cannot allocate protected host metadata."),NULL);p->bytes=sizeof(*p);
 char* compiler=xidePathResolveLexical(NULL,seed->compiler);char* parent=compiler?xidePathDirectory(compiler):NULL;
 int okay=compiler&&parent&&runtime(p,seed->runtimeDirectory,e)&&runtime(p,parent,e)&&add(p,NULL,compiler,0,e)&&add(p,NULL,seed->storageDirectory,1,e)&&add(p,NULL,seed->sessionDirectory,1,e)&&add(p,NULL,seed->preferences,0,e)&&add(p,NULL,seed->workspace,0,e)&&add(p,NULL,seed->projectSession,0,e);
 for(size_t i=0;okay&&i<seed->projects;++i)okay=seed->manifests[i]&&add(p,NULL,seed->manifests[i],0,e);
 free(compiler);free(parent);if(!okay){if(e&&!e->code)fail(e,XIDE_MODEL_INVALID,"Cannot capture the complete protected host path set.");xideNativeProtectionRelease(p);return NULL;}
 qsort(p->rows,p->count,sizeof(*p->rows),order);for(size_t i=0;i<p->count;++i)p->paths[i]=(xide_edit_alias_exclusion){p->rows[i].path,p->rows[i].directory};return p;
}
size_t xideNativeProtectionCount(const xide_native_protection* p){return p?p->count:0;}
const xide_edit_alias_exclusion* xideNativeProtectionPaths(const xide_native_protection* p){return p?p->paths:NULL;}
int xideNativeProtectionEqual(const xide_native_protection* a,const xide_native_protection* b)
{if(!a||!b||a->count!=b->count)return 0;for(size_t i=0;i<a->count;++i)if(a->rows[i].directory!=b->rows[i].directory||xidePathNativeKeyCompare(a->rows[i].key,b->rows[i].key))return 0;return 1;}
