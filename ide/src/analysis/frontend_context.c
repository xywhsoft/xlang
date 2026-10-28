#include "analysis/frontend_context.h"
#include "analysis/frontend_completion_pool.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static _Thread_local xls_frontend_context_scope* current;
typedef struct namespace_entry { xarray names; } namespace_entry;
typedef struct namespace_names { xmap entries; size_t count,bytes; } namespace_names;
static void dropNamespaceEntry(xbytesview key,void* value,void* user)
{
    (void)key; (void)user; namespace_entry* entry=value;
    for(size_t i=0;i<entry->names.Count;++i) xrtFree(*(char**)xrtArrayGet(&entry->names,i));
    xrtArrayUnit(&entry->names);
}
static int fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message); return 0; }
int xlsFrontendContextValid(const xls_context* context,const char* entry,char* error,size_t capacity)
{
    if(!context) return 1;
    const xls_context_spec* spec=xlsContextSpec(context);
    if(spec->kind==XLS_CONTEXT_C || !xrtPathIsAbs(spec->directory) || !xrtPathIsAbs(spec->entry) ||
        !entry || strcmp(spec->entry,entry) || (spec->packagePath && strlen(spec->packagePath)>32767))
        return fail(error,capacity,"native context requires an absolute xlang entry/build directory and bounded package environment");
    if(strcmp(spec->targetKind,"exe") && strcmp(spec->targetKind,"dll") && strcmp(spec->targetKind,"shared"))
        return fail(error,capacity,"unsupported native context target kind");
    /* Members define index scope, not implicit imports. The current checker
     * must not pretend it applied C/resource/include/link build settings. */
    for(size_t i=1;i<XLS_CONTEXT_LIST_COUNT;++i) if(spec->lists[i].count)
        return fail(error,capacity,"native context build options require additional frontend support");
    return 1;
}
int xlsFrontendContextsEqual(const xls_context* a,const xls_context* b)
{ return a==b || (a && b && xlsContextEqual(a,b)); }
void xlsFrontendContextBegin(xls_frontend_context_scope* scope,const xls_context* context)
{ *scope=(xls_frontend_context_scope){.context=context,.previous=current}; current=scope; }
int xlsFrontendContextEnd(xls_frontend_context_scope* scope)
{
    if(current!=scope) return 0;
    int okay=!scope->failed && !scope->completionPool;
    if(scope->completionPool)xlsCompletionPoolAbort(&scope->completionPool);
    namespace_names* names=scope->namespaceNames;
    if(names) { xrtMapUnit(&names->entries); xrtFree(names); }
    scope->namespaceNames=NULL; current=scope->previous; return okay;
}
const xls_context* xlsFrontendContextCurrent(void) { return current ? current->context : NULL; }
struct xls_completion_pool_scope** xlsFrontendCompletionPoolSlot(void)
{return current ? &current->completionPool : NULL;}
char* xlsFrontendContextPackagePath(void)
{
    const xls_context* context=xlsFrontendContextCurrent();
    if(!context) return xrtEnvGet("XLANG_PACKAGE_PATH");
    const char* value=xlsContextSpec(context)->packagePath;
    char* result=value ? xrtStrDup(value) : NULL;
    if(value && !result) current->failed|=1;
    return result;
}
char* xlsFrontendContextAbsolute(const char* path)
{
    const xls_context* context=xlsFrontendContextCurrent();
    if(!context) return xrtPathAbs(path);
    char* result=path && *path ? (xrtPathIsAbs(path) ? xrtPathAbs(path) : xrtPathJoin(xlsContextSpec(context)->directory,path)) : NULL;
    /* Windows drive-relative paths retain inherited per-drive process state.
     * Reject unresolved rooted results rather than consulting that state. */
    if(!result || !xrtPathIsAbs(result)) { xrtFree(result); current->failed|=1; return NULL; }
    return result;
}
int xlsFrontendNamespaceReady(void) { return current!=NULL; }
int xlsFrontendNamespaceActive(void) { return current && current->namespaceNames; }
int xlsFrontendNamespaceStart(void)
{
    if(!current || current->namespaceNames) { if(current) current->failed|=2; return 0; }
    namespace_names* names=xrtCalloc(1,sizeof(*names));
    if(!names) { current->failed|=2; return 0; }
    if(!xrtMapInit(&names->entries,sizeof(namespace_entry)) || !xrtMapSetDrop(&names->entries,dropNamespaceEntry,NULL)) {
        xrtMapUnit(&names->entries); xrtFree(names); current->failed|=2; return 0;
    }
    current->namespaceNames=names; return 1;
}
int xlsFrontendNamespaceAdd(const char* internal,const char* display)
{
    if(!xlsFrontendNamespaceActive() || !internal || !display) { if(current) current->failed|=2; return 0; }
    namespace_names* names=current->namespaceNames; size_t keyBytes=strlen(internal),labelBytes=strlen(display);
    if(!keyBytes || keyBytes>65536 || !labelBytes || labelBytes>512) { current->failed|=2; return 0; }
    xbytesview key={(const unsigned char*)internal,keyBytes};
    namespace_entry* entry=xrtMapGet(&names->entries,key);
    if(entry) for(size_t i=0;i<entry->names.Count;++i)
        if(!strcmp(*(char**)xrtArrayGet(&entry->names,i),display)) return 1;
    if(names->count>=4096 || keyBytes+labelBytes+2>4u*1024u*1024u-names->bytes) { current->failed|=2; return 0; }
    char* copy=xrtStrDup(display);
    if(!copy) { current->failed|=2; return 0; }
    namespace_entry fresh={0};
    if(!entry) {
        if(!xrtArrayInit(&fresh.names,sizeof(char*))) goto failed;
        entry=&fresh;
    }
    if(!xrtArrayPush(&entry->names,&copy)) goto failed;
    if(entry==&fresh && !xrtMapSet(&names->entries,key,&fresh)) {
        dropNamespaceEntry(key,&fresh,NULL); current->failed|=2; return 0;
    }
    ++names->count; names->bytes+=keyBytes+labelBytes+2; return 1;
failed:
    xrtFree(copy); if(entry==&fresh) xrtArrayUnit(&fresh.names); current->failed|=2; return 0;
}
static const namespace_entry* namespaceEntry(const char* internal)
{
    if(!xlsFrontendNamespaceActive() || !internal) return NULL;
    const namespace_names* names=current->namespaceNames;
    return xrtMapConstGet(&names->entries,(xbytesview){(const unsigned char*)internal,strlen(internal)});
}
size_t xlsFrontendNamespaceCount(const char* internal)
{ const namespace_entry* entry=namespaceEntry(internal); return entry ? entry->names.Count : 0; }
const char* xlsFrontendNamespaceName(const char* internal,size_t index)
{
    const namespace_entry* entry=namespaceEntry(internal);
    return entry && index<entry->names.Count ? *(char* const*)xrtArrayConstGet(&entry->names,index) : NULL;
}
