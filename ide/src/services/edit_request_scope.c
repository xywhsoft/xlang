#include "services/edit_request.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>

typedef struct scope_capture {
 const xide_edit_request* request;int (*cancelled)(void*);void* user;
 xide_analysis_catalog* catalog;xls_context* files[64];wchar_t* keys[64];size_t count;
 enum xide_rename_scope_error error;
} scope_capture;
static int scopeStopped(void* raw)
{scope_capture* c=raw;return xideEditRequestInvalidated(c->request)||(c->cancelled&&c->cancelled(c->user));}
static int scopeFile(scope_capture* c,const char* path)
{
 if(scopeStopped(c)){c->error=XIDE_RENAME_SCOPE_CANCELLED;return 0;}
 if(!path||!xidePathHasExtension(path,".xl"))return 1;
 if(!xrtPathIsAbs(path)){c->error=XIDE_RENAME_SCOPE_ARGUMENT;return 0;}
 wchar_t* key=xidePathNativeWide(path);if(!key){c->error=XIDE_RENAME_SCOPE_MEMORY;return 0;}
 uint64_t owners=0;int okay=xideAnalysisCatalogOwnersNative(c->catalog,key,&owners);
 if(!okay){free(key);c->error=XIDE_RENAME_SCOPE_ARGUMENT;return 0;}
 if(owners){free(key);return 1;}
 for(size_t i=0;i<c->count;++i)if(!xidePathNativeKeyCompare(key,c->keys[i])){free(key);return 1;}
 if(c->count>=64-xideAnalysisCatalogCount(c->catalog)){free(key);c->error=XIDE_RENAME_SCOPE_LIMIT;return 0;}
 xls_context* file=xideAnalysisCatalogFile(c->catalog,path);
 if(!file){free(key);c->error=XIDE_RENAME_SCOPE_MEMORY;return 0;}
 c->files[c->count]=file;c->keys[c->count++]=key;return 1;
}
xide_rename_scope* xideEditRequestScope(const xide_edit_request* p,size_t budget,int(*cancelled)(void*),void* user,enum xide_rename_scope_error* why)
{
 scope_capture c={.request=p,.cancelled=cancelled,.user=user,.error=XIDE_RENAME_SCOPE_ARGUMENT};xide_rename_scope* result=NULL;
 if(!p)goto done;
 if(scopeStopped(&c)){c.error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
 c.catalog=xideAnalysisCatalogCreate(xideEditRequestCatalog(p),NULL,0);if(!c.catalog)goto done;
 for(size_t i=0;i<xideEditRequestWorkspaceCount(p);++i)if(!scopeFile(&c,xideEditRequestWorkspaceAt(p,i)))goto done;
 /* The request owns this immutable roster; no Document or GUI is consulted. */
 for(size_t i=0;i<xideEditRequestModelCount(p);++i){
  if(scopeStopped(&c)){c.error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
  xide_edit_request_model m={0};
  if(!xideEditRequestModelAt(p,i,&m))goto done;
  if(m.open&&m.language&&!strcmp(m.language,"xlang")&&!scopeFile(&c,m.path))goto done;
 }
 result=xideRenameScopeCreate(c.catalog,c.files,c.count,budget,scopeStopped,&c,&c.error);
 if(result&&scopeStopped(&c)){xideRenameScopeRelease(result);result=NULL;c.error=XIDE_RENAME_SCOPE_CANCELLED;}
done:
 for(size_t i=0;i<c.count;++i){xlsContextRelease(c.files[i]);free(c.keys[i]);}
 xideAnalysisCatalogDestroy(c.catalog);if(why)*why=c.error;return result;
}
