#include "analysis/context.h"
#include <xrt.h>
#include <string.h>

#define CONTEXT_BYTES (16u*1024u*1024u)
#define CONTEXT_ITEMS 65536u
#define CONTEXT_STRING 262144u
enum { SCALARS=7, HEADER_WORDS=2+XLS_CONTEXT_LIST_COUNT };
struct xls_context {
    xatomic32 references;
    xls_context_spec spec;
    size_t bytes,canonicalBytes;
    const unsigned char* canonical;
    uint64_t hash;
};
static void contextError(enum xls_context_error* error,enum xls_context_error value)
{ if(error) *error=value; }
static int measure(const char* value,size_t* bytes,enum xls_context_error* error)
{
    if(!value) value="";
    size_t length=0;
    while(length<=CONTEXT_STRING && value[length]) ++length;
    if(length>CONTEXT_STRING || length+1>CONTEXT_BYTES-*bytes) {
        contextError(error,XLS_CONTEXT_LIMIT); return 0;
    }
    if(!xrtUtf8Valid((xstrview){value,length},NULL)) {
        contextError(error,XLS_CONTEXT_ENCODING); return 0;
    }
    *bytes+=length+1; return 1;
}
static const char* store(char** cursor,const char* value)
{
    if(!value) value="";
    size_t size=strlen(value)+1; char* result=*cursor;
    memcpy(result,value,size); *cursor+=size; return result;
}
xls_context* xlsContextCreate(const xls_context_spec* spec,size_t limit,enum xls_context_error* error)
{
    contextError(error,XLS_CONTEXT_ARGUMENT);
    if(!spec || spec->kind<XLS_CONTEXT_XLANG || spec->kind>XLS_CONTEXT_PROJECT ||
        !spec->entry || !*spec->entry || !spec->directory || !*spec->directory ||
        !spec->analysisProfile || !*spec->analysisProfile || !spec->targetKind || !*spec->targetKind ||
        (spec->kind==XLS_CONTEXT_PROJECT ? !spec->project || !*spec->project :
            (spec->project && *spec->project) || (spec->configuration && *spec->configuration))) return NULL;
    const char* values[]={spec->project,spec->configuration,spec->entry,spec->directory,
        spec->analysisProfile,spec->targetKind,spec->packagePath};
    size_t strings=0,items=0;
    for(size_t i=0;i<SCALARS;++i) if(!measure(values[i],&strings,error)) return NULL;
    for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) {
        const xls_context_strings* list=&spec->lists[i];
        if(list->count>CONTEXT_ITEMS-items) { contextError(error,XLS_CONTEXT_LIMIT); return NULL; }
        if(list->count && !list->items) return NULL;
        items+=list->count;
        for(size_t j=0;j<list->count;++j) {
            if(!list->items[j]) return NULL;
            if(!measure(list->items[j],&strings,error)) return NULL;
        }
    }
    size_t metadata=sizeof(xls_context)+HEADER_WORDS*sizeof(uint64_t)+items*sizeof(const char*);
    if(metadata>CONTEXT_BYTES || strings>CONTEXT_BYTES-metadata || metadata+strings>limit) {
        contextError(error,XLS_CONTEXT_LIMIT); return NULL;
    }
    xls_context* result=xrtMalloc(metadata+strings);
    if(!result) { contextError(error,XLS_CONTEXT_MEMORY); return NULL; }
    memset(result,0,sizeof(*result)); xrtAtomic32Init(&result->references,1);
    const char** pointers=(const char**)(result+1);
    /* The canonical header is copied bytewise, without an alignment assumption. */
    unsigned char* canonical=(unsigned char*)(pointers+items);
    uint64_t header[HEADER_WORDS]={0}; header[0]=(uint64_t)spec->kind; header[1]=spec->packagePath!=NULL;
    for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) header[2+i]=(uint64_t)spec->lists[i].count;
    memcpy(canonical,header,sizeof(header)); char* cursor=(char*)canonical+sizeof(header);
    result->spec.kind=spec->kind;
    const char** fields[]={&result->spec.project,&result->spec.configuration,&result->spec.entry,
        &result->spec.directory,&result->spec.analysisProfile,&result->spec.targetKind,&result->spec.packagePath};
    for(size_t i=0;i<SCALARS;++i) *fields[i]=store(&cursor,values[i]);
    if(!spec->packagePath) result->spec.packagePath=NULL;
    for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) {
        size_t count=spec->lists[i].count;
        result->spec.lists[i]=(xls_context_strings){count ? pointers : NULL,count};
        for(size_t j=0;j<count;++j) *pointers++=store(&cursor,spec->lists[i].items[j]);
    }
    result->canonical=canonical; result->canonicalBytes=sizeof(header)+strings;
    result->bytes=metadata+strings; result->hash=xrtHash64(canonical,result->canonicalBytes);
    contextError(error,XLS_CONTEXT_OK); return result;
}
xls_context* xlsContextRetain(xls_context* context)
{ if(context) xrtAtomic32FetchAdd(&context->references,1,XMEMORY_RELAXED); return context; }
void xlsContextRelease(xls_context* context)
{ if(context && xrtAtomic32FetchSub(&context->references,1,XMEMORY_ACQ_REL)==1) xrtFree(context); }
const xls_context_spec* xlsContextSpec(const xls_context* context)
{ return context ? &context->spec : NULL; }
size_t xlsContextBytes(const xls_context* context) { return context ? context->bytes : 0; }
uint64_t xlsContextHash(const xls_context* context) { return context ? context->hash : 0; }
int xlsContextEqual(const xls_context* left,const xls_context* right)
{
    return left && right && (left==right || (left->hash==right->hash &&
        left->canonicalBytes==right->canonicalBytes && !memcmp(left->canonical,right->canonical,left->canonicalBytes)));
}
