#include "services/command_catalog.h"
#include "services/catalog_sources.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static xvalue* get(const xvalue* root,const char* key) { return xrtValueObjectGet(root,sv(key)); }
static char* string(const xvalue* value)
{
    xstrview text={0}; if(!value || !xrtValueGetString(value,&text) || text.Size>INT_MAX || memchr(text.Data,0,text.Size)) return NULL;
    if(text.Size && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.Data,(int)text.Size,NULL,0)) return NULL;
    char* result=malloc(text.Size+1); if(result) { memcpy(result,text.Data,text.Size); result[text.Size]=0; } return result;
}
static int fail(xide_model_error* error,int code,const char* text)
{
    if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",text); } return 0;
}
static void clearCommand(xide_command* command)
{
    free(command->id); free(command->name); free(command->language); free(command->category); free(command->module); free(command->kind); free(command->signature); free(command->description); free(command->returns); free(command->example); free(command->insert); free(command->version); free(command->requiredImport); free(command->source);
    if(command->parameters) for(size_t i=0;i<command->parameterCount;++i) { xide_command_parameter* p=&command->parameters[i]; free(p->name); free(p->type); free(p->defaultValue); free(p->description); }
    free(command->parameters); free(command->documents);
}
void xideCatalogClear(xide_command_catalog* catalog)
{
    if(!catalog) return;
    for(size_t i=0;i<catalog->count;++i) clearCommand(&catalog->entries[i]);
    if(catalog->documents) for(size_t i=0;i<catalog->documentCount;++i) { free(catalog->documents[i].id); free(catalog->documents[i].title); free(catalog->documents[i].markdown); free(catalog->documents[i].source); }
    free(catalog->documents);
    free(catalog->entries); free(catalog->path); xrtValueRelease(catalog->source); memset(catalog,0,sizeof(*catalog));
}
static int readCommand(xide_command* item,const xvalue* root)
{
    if(!root || !xrtValueIs(root,XVALUE_OBJECT)) return 0;
#define FIELD(field,key) do { item->field=string(get(root,key)); if(!item->field) return 0; } while(0)
    FIELD(id,"id"); FIELD(name,"name"); FIELD(language,"language"); FIELD(category,"category"); FIELD(module,"module"); FIELD(kind,"kind"); FIELD(signature,"signature"); FIELD(description,"description"); FIELD(returns,"returns"); FIELD(example,"example"); FIELD(insert,"insert"); FIELD(version,"version"); FIELD(requiredImport,"requiredImport"); FIELD(source,"source");
#undef FIELD
    if(!*item->id || !*item->name || !*item->module || !*item->source || (strcmp(item->language,"xlang") && strcmp(item->language,"c"))) return 0;
    if(strcmp(item->category,"language") && strcmp(item->category,"standard") && strcmp(item->category,"extension")) return 0;
    if(strcmp(item->kind,"keyword") && strcmp(item->kind,"function") && strcmp(item->kind,"method") && strcmp(item->kind,"type") && strcmp(item->kind,"constant") && strcmp(item->kind,"module")) return 0;
    int64 line=0; uint64 unsignedLine=0;
    if(!xrtValueGetInt(get(root,"sourceLine"),&line)) { if(!xrtValueGetUInt(get(root,"sourceLine"),&unsignedLine) || unsignedLine>INT_MAX) return 0; line=(int64)unsignedLine; }
    if(line<1 || line>INT_MAX) return 0;
    item->sourceLine=(int)line;
    xvalue* params=get(root,"parameters"); if(!params || !xrtValueIs(params,XVALUE_ARRAY) || xrtValueCount(params)>4096) return 0;
    item->parameterCount=xrtValueCount(params); item->parameters=calloc(item->parameterCount+1,sizeof(*item->parameters)); if(!item->parameters) return 0;
    for(size_t i=0;i<item->parameterCount;++i) {
        xvalue* root=xrtValueArrayGet(params,i); xide_command_parameter* p=&item->parameters[i]; bool optional=false;
        if(!root || !xrtValueIs(root,XVALUE_OBJECT)) return 0;
        p->name=string(get(root,"name")); p->type=string(get(root,"type")); p->defaultValue=string(get(root,"default")); p->description=string(get(root,"description"));
        if(!p->name || !p->type || !p->defaultValue || !p->description || !xrtValueGetBool(get(root,"optional"),&optional)) return 0;
        p->optional=optional;
    }
    return 1;
}
const xide_help_document* xideCatalogDocument(const xide_command_catalog* catalog,const char* id)
{
    if(!catalog || !id) return NULL;
    for(size_t i=0;i<catalog->documentCount;++i) if(!strcmp(catalog->documents[i].id,id)) return &catalog->documents[i];
    return NULL;
}
static int readDocuments(xide_command_catalog* catalog)
{
    xvalue* roots=get(catalog->source,"documents"); if(!roots) return 1;
    if(!xrtValueIs(roots,XVALUE_ARRAY) || xrtValueCount(roots)>5000) return 0;
    catalog->documentCount=xrtValueCount(roots); catalog->documents=calloc(catalog->documentCount+1,sizeof(*catalog->documents)); if(!catalog->documents) return 0;
    for(size_t i=0;i<catalog->documentCount;++i) {
        xvalue* root=xrtValueArrayGet(roots,i); xide_help_document* doc=&catalog->documents[i]; int64 line=0; uint64 unsignedLine=0;
        if(!root || !xrtValueIs(root,XVALUE_OBJECT)) return 0;
        doc->id=string(get(root,"id")); doc->title=string(get(root,"title")); doc->markdown=string(get(root,"markdown")); doc->source=string(get(root,"source"));
        if(!doc->id || !*doc->id || !doc->title || !*doc->title || !doc->markdown || !*doc->markdown || strlen(doc->markdown)>4u*1024u*1024u || !doc->source || !*doc->source ) return 0;
        if(!xrtValueGetInt(get(root,"sourceLine"),&line)) { if(!xrtValueGetUInt(get(root,"sourceLine"),&unsignedLine) || unsignedLine>INT_MAX) return 0; line=(int64)unsignedLine; }
        if(line<1 || line>INT_MAX) return 0;
        doc->sourceLine=(int)line;
        for(size_t j=0;j<i;++j) if(!strcmp(doc->id,catalog->documents[j].id)) return 0;
    }
    return 1;
}
static int readSections(xide_command_catalog* catalog,xide_command* command,const xvalue* root)
{
    xvalue* refs=get(root,"sections"); if(!refs) return 1;
    if(!xrtValueIs(refs,XVALUE_ARRAY) || xrtValueCount(refs)>500) return 0;
    command->documentCount=xrtValueCount(refs); command->documents=calloc(command->documentCount+1,sizeof(*command->documents)); if(!command->documents) return 0;
    for(size_t i=0;i<command->documentCount;++i) {
        char* id=string(xrtValueArrayGet(refs,i)); if(!id) return 0;
        const xide_help_document* doc=xideCatalogDocument(catalog,id); free(id); if(!doc) return 0; command->documents[i]=doc;
        for(size_t j=0;j<i;++j) if(command->documents[j]==doc) return 0;
    }
    return 1;
}
static int compare(const void* a,const void* b) { return strcmp((*(const xide_command* const*)a)->id,(*(const xide_command* const*)b)->id); }
int xideCatalogLoad(xide_command_catalog* catalog,const char* path,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!catalog || !path) return fail(error,XIDE_MODEL_INVALID,"Invalid catalog arguments.");
    xide_command_catalog fresh={0}; fresh.path=xidePathResolve(NULL,path); if(!fresh.path) return fail(error,XIDE_MODEL_INVALID,"Invalid catalog path.");
    char* native=xidePathNativeUtf8(fresh.path);
    size_t size=0; unsigned char* data=native ? xrtFileReadAllLimit(native,64u*1024u*1024u,&size) : NULL;
    free(native);
    if(!data) { xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_IO,"Cannot read command catalog; verify data/commands.json is packaged with the program."); }
    size_t offset=size>=3 && data[0]==0xef && data[1]==0xbb && data[2]==0xbf ? 3 : 0;
    if(!xideCatalogDigest(data,size,fresh.sha256)) { xrtFree(data); xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_IO,"Cannot verify command catalog digest."); }
    fresh.source=xrtJsonParse((xstrview){(char*)data+offset,size-offset}); xrtFree(data);
    char* format=fresh.source && xrtValueIs(fresh.source,XVALUE_OBJECT) ? string(get(fresh.source,"format")) : NULL;
    xvalue* entries=format ? get(fresh.source,"entries") : NULL;
    int valid=format && !strcmp(format,"xlang-command-catalog-1") && entries && xrtValueIs(entries,XVALUE_ARRAY) && xrtValueCount(entries)>0 && xrtValueCount(entries)<=50000;
    free(format);
    if(!valid) { xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_INVALID,"Invalid or unsupported command catalog."); }
    if(!readDocuments(&fresh)) { xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_INVALID,"Help document metadata is invalid, duplicated or has invalid UTF-8."); }
    fresh.count=xrtValueCount(entries); fresh.entries=calloc(fresh.count,sizeof(*fresh.entries)); const xide_command** sorted=malloc(fresh.count*sizeof(*sorted));
    if(!fresh.entries || !sorted) { free(sorted); if(!fresh.entries) fresh.count=0; xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_MEMORY,"Cannot allocate command catalog."); }
    for(size_t i=0;i<fresh.count;++i) {
        if(!readCommand(&fresh.entries[i],xrtValueArrayGet(entries,i)) || !readSections(&fresh,&fresh.entries[i],xrtValueArrayGet(entries,i))) { char message[512]; snprintf(message,sizeof(message),"Command entry %zu (%s) has invalid fields, parameters or UTF-8.",i,fresh.entries[i].id ? fresh.entries[i].id : "unknown"); free(sorted); xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_INVALID,message); }
        sorted[i]=&fresh.entries[i];
    }
    qsort(sorted,fresh.count,sizeof(*sorted),compare);
    for(size_t i=1;i<fresh.count;++i) if(!strcmp(sorted[i-1]->id,sorted[i]->id)) { free(sorted); xideCatalogClear(&fresh); return fail(error,XIDE_MODEL_CONFLICT,"Duplicate stable command ID."); }
    free(sorted); xideCatalogClear(catalog); *catalog=fresh; return 1;
}
const xide_command* xideCatalogFind(const xide_command_catalog* catalog,const char* id)
{
    if(!catalog || !id) return NULL;
    for(size_t i=0;i<catalog->count;++i) if(!strcmp(catalog->entries[i].id,id)) return &catalog->entries[i];
    return NULL;
}
static int contains(const char* value,const char* word,size_t size)
{
    for(const unsigned char* p=(const unsigned char*)value;*p;++p) {
        size_t i=0; while(i<size && p[i] && (p[i]>=128 || (unsigned char)word[i]>=128 ? p[i]==(unsigned char)word[i] : tolower(p[i])==tolower((unsigned char)word[i]))) ++i;
        if(i==size) return 1;
    }
    return 0;
}
int xideCatalogMatches(const xide_command* command,const char* query)
{
    if(!command || !query) return 0;
    const char* p=query;
    while(*p) {
        while(*p && isspace((unsigned char)*p)) ++p;
        const char* begin=p; while(*p && !isspace((unsigned char)*p)) ++p; size_t size=(size_t)(p-begin); if(!size) break;
        if(!contains(command->name,begin,size) && !contains(command->module,begin,size) && !contains(command->signature,begin,size) && !contains(command->description,begin,size)) return 0;
    }
    return 1;
}
int xideCatalogCanComplete(const xide_command* command,const char* language)
{
    return command && language && !strcmp(command->language,language) && strcmp(command->kind,"method") && strcmp(command->kind,"module");
}
