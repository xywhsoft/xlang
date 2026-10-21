#include "model/workspace.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <objbase.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MANIFEST_LIMIT (16u*1024u*1024u)

static int fail(xide_model_error* error,int code,const char* format,...)
{
    if(error) { error->code=code; va_list args; va_start(args,format); vsnprintf(error->message,sizeof(error->message),format,args); va_end(args); }
    return 0;
}
static void success(xide_model_error* error) { if(error) memset(error,0,sizeof(*error)); }
static char* copy(const char* text)
{
    if(!text) text="";
    size_t size=strlen(text)+1; char* result=malloc(size); if(result) memcpy(result,text,size); return result;
}
static int reserve(void** items,size_t* capacity,size_t count,size_t size)
{
    if(count<=*capacity) return 1;
    size_t next=*capacity ? *capacity : 8;
    while(next<count) { if(next>SIZE_MAX/2) { next=count; break; } next*=2; }
    if(next>SIZE_MAX/size) return 0;
    void* result=realloc(*items,next*size); if(!result) return 0;
    *items=result; *capacity=next; return 1;
}
static xstrview view(const char* text) { return (xstrview){text,strlen(text)}; }
static xvalue* get(const xvalue* object,const char* key) { return object ? xrtValueObjectGet(object,view(key)) : NULL; }
static int isObject(const xvalue* value) { return value && xrtValueIs(value,XVALUE_OBJECT); }
static int isArray(const xvalue* value) { return value && xrtValueIs(value,XVALUE_ARRAY); }
static int setString(xvalue* object,const char* key,const char* text)
{
    return xrtValueObjectSetNew(object,view(key),xrtValueString(view(text ? text : "")));
}
static char* stringValue(const xvalue* value)
{
    xstrview text;
    if(!value || !xrtValueGetString(value,&text) || memchr(text.Data,0,text.Size)) return NULL;
    char* result=malloc(text.Size+1); if(result) { memcpy(result,text.Data,text.Size); result[text.Size]=0; }
    return result;
}
static int readString(const xvalue* object,const char* key,const char* fallback,int nonempty,char** output,xide_model_error* error)
{
    xvalue* value=get(object,key);
    *output=value ? stringValue(value) : fallback ? copy(fallback) : NULL;
    if(!*output || (nonempty && !**output)) return fail(error,XIDE_MODEL_INVALID,"Invalid or missing string: %s",key);
    return 1;
}
static char* makeId(const char* prefix)
{
    GUID guid; if(FAILED(CoCreateGuid(&guid))) return NULL;
    char buffer[64]; snprintf(buffer,sizeof(buffer),"%s-%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",prefix,
        (unsigned long)guid.Data1,guid.Data2,guid.Data3,guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    return copy(buffer);
}
static void stringsClear(xide_strings* list)
{
    for(size_t i=0;i<list->count;++i) free(list->items[i]);
    free(list->items); memset(list,0,sizeof(*list));
}
static int stringsAdd(xide_strings* list,const char* text)
{
    char* item=copy(text); if(!item) return 0;
    if(!reserve((void**)&list->items,&list->capacity,list->count+1,sizeof(*list->items))) { free(item); return 0; }
    list->items[list->count++]=item; return 1;
}
static int stringsHasPath(const xide_strings* list,const char* path)
{
    for(size_t i=0;i<list->count;++i) if(xidePathEqual(list->items[i],path)) return 1;
    return 0;
}
static void stringsRemovePath(xide_strings* list,const char* path)
{
    size_t out=0;
    for(size_t i=0;i<list->count;++i) if(xidePathEqual(list->items[i],path)) free(list->items[i]); else list->items[out++]=list->items[i];
    list->count=out;
}
static void fileClear(xide_file* file)
{
    free(file->id); free(file->path); free(file->kind); xrtValueRelease(file->source); memset(file,0,sizeof(*file));
}
static void filesClear(xide_files* files)
{
    for(size_t i=0;i<files->count;++i) fileClear(&files->items[i]);
    free(files->items); memset(files,0,sizeof(*files));
}
static xide_file* fileByPath(xide_files* files,const char* path)
{
    int exists=xidePathExists(path);
    for(size_t i=0;i<files->count;++i) if(!strcmp(files->items[i].path,path) || (exists && xidePathEqual(files->items[i].path,path))) return &files->items[i];
    return NULL;
}
static const char* fileKind(const char* path)
{
    if(xidePathHasExtension(path,".xl")) return "xlang";
    if(xidePathHasExtension(path,".c")) return "c";
    if(xidePathHasExtension(path,".h")) return "header";
    return "text";
}
static int filesAdd(xide_files* files,const char* id,const char* path,const char* kind,const xvalue* source,xide_model_error* error)
{
    int exists=xidePathExists(path);
    for(size_t i=0;i<files->count;++i) {
        if(id && !strcmp(id,files->items[i].id)) return fail(error,XIDE_MODEL_CONFLICT,"Duplicate file ID: %s",id);
        if(!strcmp(path,files->items[i].path) || (exists && xidePathEqual(path,files->items[i].path))) return fail(error,XIDE_MODEL_CONFLICT,"Duplicate file path: %s",path);
    }
    xide_file file={0}; file.id=id ? copy(id) : makeId("file"); file.path=copy(path);
    file.kind=copy(kind ? kind : fileKind(path)); file.source=source ? xrtValueClone(source) : xrtValueObject();
    if(!file.id || !file.path || !file.kind || !file.source || !reserve((void**)&files->items,&files->capacity,files->count+1,sizeof(file))) {
        fileClear(&file); return fail(error,XIDE_MODEL_MEMORY,"Out of memory adding file.");
    }
    files->items[files->count++]=file; return 1;
}
static int ensureFile(xide_files* files,const char* path,const char* kind,xide_model_error* error)
{
    xide_file* existing=fileByPath(files,path);
    if(existing) {
        char* changed=copy(kind); if(!changed) return fail(error,XIDE_MODEL_MEMORY,"Out of memory updating file kind.");
        free(existing->kind); existing->kind=changed; return 1;
    }
    return filesAdd(files,NULL,path,kind,NULL,error);
}
static void configurationClear(xide_configuration* config)
{
    free(config->test.program); free(config->test.workingDirectory); stringsClear(&config->test.arguments);
    xrtValueRelease(config->test.environment); xrtValueRelease(config->test.source);
    free(config->id); free(config->name); free(config->output); free(config->workingDirectory);
    stringsClear(&config->includePaths); stringsClear(&config->libraryPaths); stringsClear(&config->linkLibraries); stringsClear(&config->arguments);
    stringsClear(&config->cAnalysis.systemIncludes); stringsClear(&config->cAnalysis.forcedIncludes); stringsClear(&config->cAnalysis.defines); stringsClear(&config->cAnalysis.undefines);
    xrtValueRelease(config->environment); xrtValueRelease(config->source); xrtValueRelease(config->runSource); memset(config,0,sizeof(*config));
}
void xideProjectClear(xide_project* project)
{
    if(!project) return;
    free(project->activeConfiguration); free(project->id); free(project->manifest); free(project->name); free(project->description); free(project->kind); free(project->entry); free(project->issue);
    stringsClear(&project->cSources); stringsClear(&project->resources); filesClear(&project->files);
    for(size_t i=0;i<project->configurationCount;++i) configurationClear(&project->configurations[i]);
    free(project->configurations); xrtValueRelease(project->source); xrtValueRelease(project->referenceSource); xrtValueRelease(project->libraries);
    memset(project,0,sizeof(*project));
}
void xideWorkspaceClear(xide_workspace* workspace)
{
    if(!workspace) return;
    free(workspace->manifest); free(workspace->name); free(workspace->startupProject); filesClear(&workspace->files);
    for(size_t i=0;i<workspace->projectCount;++i) xideProjectClear(&workspace->projects[i]);
    free(workspace->projects); xrtValueRelease(workspace->source); memset(workspace,0,sizeof(*workspace));
}
xide_project* xideWorkspaceFindProject(xide_workspace* workspace,const char* id)
{
    if(!workspace || !id) return NULL;
    for(size_t i=0;i<workspace->projectCount;++i) if(!strcmp(workspace->projects[i].id,id)) return &workspace->projects[i];
    return NULL;
}
static xvalue* readManifest(const char* path,xide_model_error* error)
{
    size_t size=0; unsigned char* data=xideIoReadAllLimit(path,MANIFEST_LIMIT,&size);
    if(!data) { fail(error,XIDE_MODEL_IO,"Cannot read manifest: %s",path); return NULL; }
    size_t start=size>=3 && data[0]==0xef && data[1]==0xbb && data[2]==0xbf ? 3 : 0;
    xvalue* root=xrtJsonParse((xstrview){(char*)data+start,size-start}); xrtFree(data);
    if(!isObject(root)) { xrtValueRelease(root); fail(error,XIDE_MODEL_INVALID,"Invalid JSON object: %s",path); return NULL; }
    return root;
}
static int readList(const xvalue* object,const char* key,const char* base,const char* extension,xide_strings* list,xide_model_error* error)
{
    xvalue* array=get(object,key); if(!array) return 1;
    if(!isArray(array)) return fail(error,XIDE_MODEL_INVALID,"Expected array: %s",key);
    for(size_t i=0;i<xrtValueCount(array);++i) {
        char* text=stringValue(xrtValueArrayGet(array,i));
        if(!text || (base && !*text)) { free(text); return fail(error,XIDE_MODEL_INVALID,"Invalid string in %s",key); }
        char* value=base ? xidePathResolve(base,text) : copy(text); free(text);
        if(!value) return fail(error,XIDE_MODEL_MEMORY,"Invalid UTF-8 path or out of memory: %s",key);
        if(extension && !xidePathHasExtension(value,extension)) { free(value); return fail(error,XIDE_MODEL_INVALID,"Wrong source extension in %s",key); }
        if(base && stringsHasPath(list,value)) { free(value); return fail(error,XIDE_MODEL_CONFLICT,"Duplicate path in %s",key); }
        int added=stringsAdd(list,value); free(value);
        if(!added) return fail(error,XIDE_MODEL_MEMORY,"Out of memory reading %s",key);
    }
    return 1;
}
static int readFiles(const xvalue* object,const char* base,xide_files* files,xide_model_error* error)
{
    xvalue* array=get(object,"files"); if(!array) return 1;
    if(!isArray(array)) return fail(error,XIDE_MODEL_INVALID,"Expected files array.");
    for(size_t i=0;i<xrtValueCount(array);++i) {
        xvalue* entry=xrtValueArrayGet(array,i); char *id=NULL,*path=NULL,*kind=NULL,*absolutePath=NULL;
        int ok=isObject(entry) && readString(entry,"id",NULL,1,&id,error) && readString(entry,"path",NULL,1,&path,error) && readString(entry,"kind","text",1,&kind,error);
        if(ok) { absolutePath=xidePathResolve(base,path); ok=absolutePath && filesAdd(files,id,absolutePath,kind,entry,error); }
        free(id); free(path); free(kind); free(absolutePath);
        if(!ok) { if(error && !error->code) fail(error,XIDE_MODEL_INVALID,"Invalid files entry."); return 0; }
    }
    return 1;
}
int xideCAnalysisMacroValid(const char* value,int define)
{
    size_t n=value ? strlen(value) : 0; if(!n || n>4096 || strpbrk(value,"\r\n") || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,(int)n,NULL,0)) return 0;
    const unsigned char* p=(const unsigned char*)value;
    if(!((*p>='A' && *p<='Z') || (*p>='a' && *p<='z') || *p=='_')) return 0;
    for(++p;*p && !(define && *p=='=');++p) if(!((*p>='A' && *p<='Z') || (*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')) return 0;
    return 1;
}
static int readCAnalysis(const xvalue* root,const char* base,xide_c_analysis_config* c,xide_model_error* e)
{
    const xvalue* v=get(root,"cAnalysis"); if(!v) return 1;
    if(!isObject(v)) return fail(e,XIDE_MODEL_INVALID,"cAnalysis must be an object.");
    const char* keys[]={"systemIncludes","forcedIncludes","defines","undefines"};
    xide_strings* lists[]={&c->systemIncludes,&c->forcedIncludes,&c->defines,&c->undefines};
    for(size_t i=0;i<4;++i) {
        const xvalue* a=get(v,keys[i]);
        if(a && (!isArray(a) || xrtValueCount(a)>128)) return fail(e,XIDE_MODEL_INVALID,"C analysis lists accept at most 128 entries: %s",keys[i]);
        if(!readList(v,keys[i],i<2 ? base : NULL,NULL,lists[i],e)) return 0;
        for(size_t j=0;j<lists[i]->count;++j) if((i<2 && strlen(lists[i]->items[j])>32768) || (i>=2 && !xideCAnalysisMacroValid(lists[i]->items[j],i==2))) return fail(e,XIDE_MODEL_INVALID,"Invalid C analysis path or object-like macro: %s",keys[i]);
    }
    return 1;
}
static int readTest(const xvalue* root,const char* base,xide_test_config* test,xide_model_error* error)
{
    xvalue* value=get(root,"test");
    if(!value) return 1;
    if(!isObject(value)) return fail(error,XIDE_MODEL_INVALID,"Test must be an object containing an explicit program.");
    char *program=NULL,*directory=NULL;
    if(!readString(value,"program","",0,&program,error)) return 0;
    test->program=*program ? xidePathResolve(base,program) : copy(""); free(program);
    if(!readString(value,"workingDirectory",".",1,&directory,error)) return 0;
    test->workingDirectory=xidePathResolve(base,directory); free(directory);
    if(!test->program || !test->workingDirectory || !readList(value,"arguments",NULL,NULL,&test->arguments,error)) return 0;
    xvalue* environment=get(value,"environment");
    if(environment && !isObject(environment)) return fail(error,XIDE_MODEL_INVALID,"Test environment must be an object.");
    for(size_t i=0;i<xrtValueCount(environment);++i) {
        xstrview key,text; xvalue* item=xrtValueObjectAt(environment,i,&key);
        if(!xrtValueGetString(item,&text) || !key.Size || memchr(key.Data,0,key.Size) || memchr(key.Data,'=',key.Size) || memchr(text.Data,0,text.Size))
            return fail(error,XIDE_MODEL_INVALID,"Test environment must map nonempty variable names to strings.");
        for(size_t j=0;j<i;++j) {
            xstrview other; xrtValueObjectAt(environment,j,&other);
            if(key.Size==other.Size && !_strnicmp(key.Data,other.Data,key.Size)) return fail(error,XIDE_MODEL_INVALID,"Duplicate test environment variable name.");
        }
    }
    test->environment=environment ? xrtValueClone(environment) : xrtValueObject(); test->source=xrtValueClone(value);
    return test->environment && test->source;
}
static int readConfiguration(const xvalue* root,const char* base,xide_configuration* config,xide_model_error* error)
{
    if(!isObject(root)) return fail(error,XIDE_MODEL_INVALID,"Invalid configuration object.");
    if(!readString(root,"id",NULL,1,&config->id,error) || !readString(root,"name",config->id,1,&config->name,error)) return 0;
    char* output=NULL;
    if(!readString(root,"output","",0,&output,error)) return 0;
    config->output=*output ? xidePathResolve(base,output) : copy(""); free(output);
    if(!config->output || !readList(root,"includePaths",base,NULL,&config->includePaths,error) ||
       !readList(root,"libraryPaths",base,NULL,&config->libraryPaths,error) || !readList(root,"linkLibraries",NULL,NULL,&config->linkLibraries,error)) return 0;
    if(!readCAnalysis(root,base,&config->cAnalysis,error) || !readTest(root,base,&config->test,error)) return 0;
    xvalue* run=get(root,"run"); if(run && !isObject(run)) return fail(error,XIDE_MODEL_INVALID,"Invalid run configuration.");
    char* directory=NULL; if(!readString(run,"workingDirectory",".",1,&directory,error)) return 0;
    config->workingDirectory=xidePathResolve(base,directory); free(directory);
    if(!config->workingDirectory || !readList(run,"arguments",NULL,NULL,&config->arguments,error)) return 0;
    xvalue* environment=get(run,"environment");
    if(environment && !isObject(environment)) return fail(error,XIDE_MODEL_INVALID,"Invalid environment object.");
    if(environment) for(size_t i=0;i<xrtValueCount(environment);++i) {
        xstrview key,value; xvalue* item=xrtValueObjectAt(environment,i,&key);
        if(!xrtValueGetString(item,&value) || memchr(value.Data,0,value.Size) || memchr(key.Data,0,key.Size) || memchr(key.Data,'=',key.Size) || !key.Size)
            return fail(error,XIDE_MODEL_INVALID,"Environment must map nonempty variable names to strings.");
    }
    config->environment=environment ? xrtValueClone(environment) : xrtValueObject();
    config->source=xrtValueClone(root); config->runSource=run ? xrtValueClone(run) : xrtValueObject();
    return config->environment && config->source && config->runSource;
}
static int projectFromJson(xide_project* project,const char* path,const xvalue* root,xide_model_error* error)
{
    if(!get(root,"format") && (get(root,"Version") || get(root,"Project"))) return fail(error,XIDE_MODEL_LEGACY,"Legacy project requires explicit import.");
    char *format=NULL,*entry=NULL,*base=xidePathDirectory(path);
    int ok=base && readString(root,"format",NULL,1,&format,error);
    if(ok && strcmp(format,"xlang-project-1")) { fail(error,get(root,"Version") || get(root,"Project") ? XIDE_MODEL_LEGACY : XIDE_MODEL_INVALID,"Unsupported project format. Import legacy projects explicitly."); ok=0; }
    if(ok) ok=readString(root,"kind","exe",1,&project->kind,error) && readString(root,"entry",NULL,1,&entry,error);
    if(ok && strcmp(project->kind,"exe") && strcmp(project->kind,"dll") && strcmp(project->kind,"shared")) { fail(error,XIDE_MODEL_INVALID,"Invalid project kind."); ok=0; }
    if(ok && !xidePathHasExtension(entry,".xl")) { fail(error,XIDE_MODEL_INVALID,"Project entry must be an .xl file."); ok=0; }
    if(ok) { project->entry=xidePathResolve(base,entry); ok=project->entry && readList(root,"cSources",base,".c",&project->cSources,error) && readList(root,"resources",base,NULL,&project->resources,error); }
    for(size_t i=0;ok && i<project->resources.count;++i) if(!xidePathHasExtension(project->resources.items[i],".json") && !xidePathHasExtension(project->resources.items[i],".xlon")) { fail(error,XIDE_MODEL_INVALID,"Compiler resources must be .json or .xlon configurations."); ok=0; }
    xvalue* ide=get(root,"ide"); if(ok && ide && !isObject(ide)) { fail(error,XIDE_MODEL_INVALID,"Invalid ide metadata."); ok=0; }
    char* ideFormat=NULL;
    if(ok && ide) {
        ok=readString(ide,"format","xlang-ide-project-1",1,&ideFormat,error);
        if(ok && strcmp(ideFormat,"xlang-ide-project-1")) { fail(error,XIDE_MODEL_INVALID,"Unsupported IDE project metadata format."); ok=0; }
    }
    if(ok) ok=readString(ide,"name",xidePathFilename(path),1,&project->name,error) && readString(ide,"description","",0,&project->description,error) && readFiles(ide,base,&project->files,error);
    if(ok) ok=ensureFile(&project->files,project->entry,"xlang",error);
    for(size_t i=0;ok && i<project->cSources.count;++i) ok=ensureFile(&project->files,project->cSources.items[i],"c",error);
    for(size_t i=0;ok && i<project->resources.count;++i) ok=ensureFile(&project->files,project->resources.items[i],"resource-config",error);
    xvalue* configs=get(ide,"configurations"); if(ok && configs && !isArray(configs)) { fail(error,XIDE_MODEL_INVALID,"Expected configurations array."); ok=0; }
    for(size_t i=0;ok && configs && i<xrtValueCount(configs);++i) {
        xide_configuration config={0}; ok=readConfiguration(xrtValueArrayGet(configs,i),base,&config,error);
        for(size_t j=0;ok && j<project->configurationCount;++j) if(!strcmp(project->configurations[j].id,config.id)) { fail(error,XIDE_MODEL_CONFLICT,"Duplicate configuration ID."); ok=0; }
        if(ok) ok=reserve((void**)&project->configurations,&project->configurationCapacity,project->configurationCount+1,sizeof(config));
        if(ok) project->configurations[project->configurationCount++]=config; else configurationClear(&config);
    }
    if(ok) ok=readString(ide,"activeConfiguration",project->configurationCount ? project->configurations[0].id : "",0,&project->activeConfiguration,error);
    if(ok && *project->activeConfiguration && !xideProjectFindConfiguration(project,project->activeConfiguration)) { fail(error,XIDE_MODEL_INVALID,"Active configuration ID does not exist."); ok=0; }
    xvalue* libraries=get(ide,"libraries"); if(ok && libraries && !isArray(libraries)) { fail(error,XIDE_MODEL_INVALID,"Expected libraries array."); ok=0; }
    for(size_t i=0;ok && libraries && i<xrtValueCount(libraries);++i) {
        xvalue* item=xrtValueArrayGet(libraries,i); char *kind=NULL,*name=NULL;
        ok=isObject(item) && readString(item,"kind",NULL,1,&kind,error) && readString(item,"name",NULL,1,&name,error);
        free(kind); free(name);
        if(!ok && error && !error->code) fail(error,XIDE_MODEL_INVALID,"Invalid library metadata.");
    }
    if(ok) { project->libraries=libraries ? xrtValueClone(libraries) : xrtValueArray(); project->source=xrtValueClone(root); project->manifest=copy(path); project->available=1; ok=project->libraries && project->source && project->manifest; }
    free(format); free(entry); free(ideFormat); free(base);
    if(!ok && error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot allocate project data.");
    return ok;
}
int xideProjectDecode(xide_project* project,const char* path,const xvalue* root,xide_model_error* error)
{
    success(error); if(!project || !path || !isObject(root)) return fail(error,XIDE_MODEL_INVALID,"Invalid project snapshot.");
    char* absolutePath=xidePathResolve(NULL,path); if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid project path.");
    xide_project temporary={0}; int ok=projectFromJson(&temporary,absolutePath,root,error); free(absolutePath);
    if(!ok) { xideProjectClear(&temporary); return 0; }
    xideProjectClear(project); *project=temporary; return 1;
}
int xideProjectLoad(xide_project* project,const char* path,xide_model_error* error)
{
    success(error); if(!project || !path) return fail(error,XIDE_MODEL_INVALID,"Invalid project load arguments.");
    char* absolutePath=xidePathResolve(NULL,path); if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid project path.");
    xvalue* root=readManifest(absolutePath,error); int ok=root && xideProjectDecode(project,absolutePath,root,error);
    xrtValueRelease(root); free(absolutePath); return ok;
}
int xideWorkspaceCreate(xide_workspace* workspace,const char* name,xide_model_error* error)
{
    success(error); if(!workspace || !name || !*name) return fail(error,XIDE_MODEL_INVALID,"Workspace name is required.");
    xide_workspace temporary={0}; temporary.name=copy(name); temporary.startupProject=copy(""); temporary.source=xrtValueObject(); temporary.dirty=1;
    if(!temporary.name || !temporary.startupProject || !temporary.source) { xideWorkspaceClear(&temporary); return fail(error,XIDE_MODEL_MEMORY,"Cannot allocate workspace."); }
    xideWorkspaceClear(workspace); *workspace=temporary; return 1;
}
static int addReference(xide_workspace* workspace,const char* id,const char* path,const xvalue* source,int allowUnavailable,int loadProjects,xide_model_error* error)
{
    if(id && xideWorkspaceFindProject(workspace,id)) return fail(error,XIDE_MODEL_CONFLICT,"Duplicate project ID: %s",id);
    for(size_t i=0;i<workspace->projectCount;++i) if(xidePathEqual(workspace->projects[i].manifest,path)) return fail(error,XIDE_MODEL_CONFLICT,"Project is already in the workspace.");
    xide_project project={0}; xide_model_error loadError={0};
    int loaded=loadProjects && xideProjectLoad(&project,path,&loadError);
    if(!loadProjects) snprintf(loadError.message,sizeof(loadError.message),"Project contents have not been read yet.");
    if(!loaded && (!allowUnavailable || loadError.code==XIDE_MODEL_MEMORY)) { if(error) *error=loadError; return 0; }
    if(!loaded) {
        project.manifest=copy(path); project.name=copy(xidePathFilename(path)); project.description=copy(""); project.activeConfiguration=copy(""); project.kind=copy(""); project.entry=copy(""); project.issue=copy(loadError.message);
        if(!project.manifest || !project.name || !project.description || !project.activeConfiguration || !project.kind || !project.entry || !project.issue) { xideProjectClear(&project); return fail(error,XIDE_MODEL_MEMORY,"Cannot retain unavailable project."); }
    }
    project.id=id ? copy(id) : makeId("project"); project.referenceSource=source ? xrtValueClone(source) : xrtValueObject();
    if(!project.id || !project.referenceSource || !reserve((void**)&workspace->projects,&workspace->projectCapacity,workspace->projectCount+1,sizeof(project))) {
        xideProjectClear(&project); return fail(error,XIDE_MODEL_MEMORY,"Cannot add project reference.");
    }
    workspace->projects[workspace->projectCount++]=project; return 1;
}
static int workspaceDecode(xide_workspace* workspace,const char* path,const xvalue* root,int loadProjects,xide_model_error* error)
{
    success(error); if(!workspace || !path || !isObject(root)) return fail(error,XIDE_MODEL_INVALID,"Invalid workspace load arguments.");
    char* absolutePath=xidePathResolve(NULL,path); if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid workspace path.");
    if(!get(root,"format") && (get(root,"Version") || get(root,"Projects"))) {
        free(absolutePath); return fail(error,XIDE_MODEL_LEGACY,"Legacy workspace requires explicit import.");
    }
    xide_workspace temporary={0}; char *base=xidePathDirectory(absolutePath),*format=NULL;
    int ok=base && readString(root,"format",NULL,1,&format,error);
    if(ok && strcmp(format,"xlang-workspace-1")) { fail(error,get(root,"Version") || get(root,"Projects") ? XIDE_MODEL_LEGACY : XIDE_MODEL_INVALID,"Unsupported workspace format. Import legacy workspaces explicitly."); ok=0; }
    if(ok) ok=readString(root,"name",NULL,1,&temporary.name,error) && readString(root,"startupProject","",0,&temporary.startupProject,error);
    xvalue* projects=get(root,"projects"); if(ok && projects && !isArray(projects)) { fail(error,XIDE_MODEL_INVALID,"Expected projects array."); ok=0; }
    for(size_t i=0;ok && projects && i<xrtValueCount(projects);++i) {
        xvalue* reference=xrtValueArrayGet(projects,i); char *id=NULL,*relativePath=NULL,*projectPath=NULL;
        ok=isObject(reference) && readString(reference,"id",NULL,1,&id,error) && readString(reference,"path",NULL,1,&relativePath,error);
        if(ok) { projectPath=xidePathResolve(base,relativePath); ok=projectPath && addReference(&temporary,id,projectPath,reference,1,loadProjects,error); }
        free(id); free(relativePath); free(projectPath);
    }
    if(ok && *temporary.startupProject && !xideWorkspaceFindProject(&temporary,temporary.startupProject)) { fail(error,XIDE_MODEL_INVALID,"startupProject must reference an existing project ID."); ok=0; }
    if(ok) ok=readFiles(root,base,&temporary.files,error);
    if(ok) { temporary.source=xrtValueClone(root); temporary.manifest=copy(absolutePath); ok=temporary.source && temporary.manifest; }
    free(format); free(base); free(absolutePath);
    if(!ok) { xideWorkspaceClear(&temporary); if(error && !error->code) fail(error,XIDE_MODEL_INVALID,"Invalid workspace data."); return 0; }
    xideWorkspaceClear(workspace); *workspace=temporary; return 1;
}
int xideWorkspaceDecode(xide_workspace* workspace,const char* path,const xvalue* root,xide_model_error* error)
{ return workspaceDecode(workspace,path,root,0,error); }
int xideWorkspaceLoad(xide_workspace* workspace,const char* path,xide_model_error* error)
{
    success(error); if(!workspace || !path) return fail(error,XIDE_MODEL_INVALID,"Invalid workspace load arguments.");
    char* absolutePath=xidePathResolve(NULL,path); if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid workspace path.");
    xvalue* root=readManifest(absolutePath,error); int ok=root && workspaceDecode(workspace,absolutePath,root,1,error);
    xrtValueRelease(root); free(absolutePath); return ok;
}
static xvalue* listJson(const xide_strings* list,const char* base)
{
    xvalue* result=xrtValueArray(); if(!result) return NULL;
    for(size_t i=0;i<list->count;++i) {
        char* text=base ? xidePathRelative(base,list->items[i]) : copy(list->items[i]);
        int ok=text && xrtValueArrayAppendNew(result,xrtValueString(view(text)));
        free(text); if(!ok) { xrtValueRelease(result); return NULL; }
    }
    return result;
}
static xvalue* filesJson(const xide_files* files,const char* base,int includeKind)
{
    xvalue* result=xrtValueArray(); if(!result) return NULL;
    for(size_t i=0;i<files->count;++i) {
        const xide_file* file=&files->items[i];
        xvalue* item=xrtValueClone(file->source); char* path=xidePathRelative(base,file->path);
        int ok=item && path && setString(item,"id",file->id) && setString(item,"path",path) && (!includeKind || setString(item,"kind",file->kind));
        free(path);
        if(ok) ok=xrtValueArrayAppendNew(result,item); else xrtValueRelease(item);
        if(!ok) { xrtValueRelease(result); return NULL; }
    }
    return result;
}
static xvalue* configurationJson(const xide_configuration* config,const char* base)
{
    xvalue *root=xrtValueClone(config->source),*run=xrtValueClone(config->runSource);
    char* output=*config->output ? xidePathRelative(base,config->output) : copy("");
    char* directory=xidePathRelative(base,config->workingDirectory);
    int ok=root && run && output && directory && setString(root,"id",config->id) && setString(root,"name",config->name) &&
        setString(root,"output",output) && xrtValueObjectSetNew(root,view("includePaths"),listJson(&config->includePaths,base)) &&
        xrtValueObjectSetNew(root,view("libraryPaths"),listJson(&config->libraryPaths,base)) &&
        xrtValueObjectSetNew(root,view("linkLibraries"),listJson(&config->linkLibraries,NULL)) && setString(run,"workingDirectory",directory) &&
        xrtValueObjectSetNew(run,view("arguments"),listJson(&config->arguments,NULL)) && xrtValueObjectSet(run,view("environment"),config->environment);
    const xide_c_analysis_config* c=&config->cAnalysis;
    if(ok && (get(config->source,"cAnalysis") || c->systemIncludes.count || c->forcedIncludes.count || c->defines.count || c->undefines.count)) {
        xvalue* analysis=get(config->source,"cAnalysis") ? xrtValueClone(get(config->source,"cAnalysis")) : xrtValueObject();
        ok=analysis && xrtValueObjectSetNew(analysis,view("systemIncludes"),listJson(&c->systemIncludes,base)) && xrtValueObjectSetNew(analysis,view("forcedIncludes"),listJson(&c->forcedIncludes,base)) && xrtValueObjectSetNew(analysis,view("defines"),listJson(&c->defines,NULL)) && xrtValueObjectSetNew(analysis,view("undefines"),listJson(&c->undefines,NULL));
        if(ok) ok=xrtValueObjectSetNew(root,view("cAnalysis"),analysis); else xrtValueRelease(analysis);
    }
    if(ok && config->test.source) {
        const xide_test_config* t=&config->test; xvalue* test=xrtValueDeepClone(t->source);
        char *program=*t->program ? xidePathRelative(base,t->program) : copy(""),*cwd=xidePathRelative(base,t->workingDirectory);
        ok=test && program && cwd && setString(test,"program",program) && setString(test,"workingDirectory",cwd) &&
            xrtValueObjectSetNew(test,view("arguments"),listJson(&t->arguments,NULL)) && xrtValueObjectSetNew(test,view("environment"),xrtValueDeepClone(t->environment));
        free(program); free(cwd);
        if(ok) ok=xrtValueObjectSetNew(root,view("test"),test); else xrtValueRelease(test);
    }
    free(output); free(directory);
    if(ok) ok=xrtValueObjectSetNew(root,view("run"),run); else xrtValueRelease(run);
    if(!ok) { xrtValueRelease(root); return NULL; }
    return root;
}
static xvalue* projectJson(const xide_project* project,const char* base)
{
    xvalue *root=xrtValueClone(project->source),*ide=get(root,"ide") ? xrtValueClone(get(root,"ide")) : xrtValueObject();
    xvalue* configs=xrtValueArray(); char* entry=xidePathRelative(base,project->entry);
    int ok=root && ide && configs && entry && setString(root,"format","xlang-project-1") && setString(root,"kind",project->kind) && setString(root,"entry",entry) &&
        xrtValueObjectSetNew(root,view("cSources"),listJson(&project->cSources,base)) && xrtValueObjectSetNew(root,view("resources"),listJson(&project->resources,base)) &&
        setString(ide,"format","xlang-ide-project-1") && setString(ide,"name",project->name) && setString(ide,"description",project->description) && setString(ide,"activeConfiguration",project->activeConfiguration ? project->activeConfiguration : "") &&
        xrtValueObjectSetNew(ide,view("files"),filesJson(&project->files,base,1)) && xrtValueObjectSet(ide,view("libraries"),project->libraries);
    for(size_t i=0;ok && i<project->configurationCount;++i) ok=xrtValueArrayAppendNew(configs,configurationJson(&project->configurations[i],base));
    free(entry);
    if(ok) ok=xrtValueObjectSetNew(ide,view("configurations"),configs); else xrtValueRelease(configs);
    if(ok) ok=xrtValueObjectSetNew(root,view("ide"),ide); else xrtValueRelease(ide);
    if(!ok) { xrtValueRelease(root); return NULL; }
    return root;
}
xvalue* xideProjectExport(const xide_project* project,xide_model_error* error)
{
    success(error); if(!project || !project->available || !project->manifest) { fail(error,XIDE_MODEL_INVALID,"No available project snapshot."); return NULL; }
    char* base=xidePathDirectory(project->manifest); xvalue* root=base ? projectJson(project,base) : NULL; free(base);
    if(!root) fail(error,XIDE_MODEL_MEMORY,"Cannot export project snapshot.");
    return root;
}
xvalue* xideWorkspaceExport(const xide_workspace* workspace,xide_model_error* error)
{
    success(error); if(!workspace || !workspace->source || !workspace->manifest) { fail(error,XIDE_MODEL_INVALID,"No named workspace snapshot."); return NULL; }
    char* base=xidePathDirectory(workspace->manifest); xvalue *root=xrtValueClone(workspace->source),*projects=xrtValueArray();
    int ok=base && root && projects && setString(root,"format","xlang-workspace-1") && setString(root,"name",workspace->name) && setString(root,"startupProject",workspace->startupProject) && xrtValueObjectSetNew(root,view("files"),filesJson(&workspace->files,base,0));
    for(size_t i=0;ok && i<workspace->projectCount;++i) {
        const xide_project* p=&workspace->projects[i]; xvalue* item=xrtValueClone(p->referenceSource); char* relative=xidePathRelative(base,p->manifest);
        ok=item && relative && setString(item,"id",p->id) && setString(item,"path",relative); free(relative);
        if(ok) ok=xrtValueArrayAppendNew(projects,item); else xrtValueRelease(item);
    }
    if(ok) ok=xrtValueObjectSetNew(root,view("projects"),projects); else xrtValueRelease(projects);
    free(base); if(!ok) { xrtValueRelease(root); fail(error,XIDE_MODEL_MEMORY,"Cannot export workspace snapshot."); return NULL; } return root;
}
static int unchangedManifest(const char* target,const char* original,const xvalue* baseline,xide_model_error* error)
{
    if(!original || !xidePathEqual(target,original) || !get(baseline,"format")) return 1;
    xide_model_error readError={0}; xvalue* current=readManifest(target,&readError);
    int unchanged=current && xrtValueEqual(current,baseline); xrtValueRelease(current);
    return unchanged ? 1 : fail(error,XIDE_MODEL_CONFLICT,"Manifest changed on disk; reload before saving: %s",target);
}
static int writeManifest(const char* path,const xvalue* root,xide_model_error* error)
{
    size_t size=0; char* text=xrtJsonStringify(root,true,&size);
    if(!text) return fail(error,XIDE_MODEL_MEMORY,"Cannot serialize manifest.");
    int ok=xideIoWriteAtomic(path,(xbytesview){(unsigned char*)text,size}); xrtFree(text);
    return ok ? 1 : fail(error,XIDE_MODEL_IO,"Atomic manifest save failed: %s",path);
}
int xideProjectSave(xide_project* project,const char* path,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !project->entry || !*project->entry) return fail(error,XIDE_MODEL_INVALID,"Cannot save an unavailable or incomplete project.");
    if(!path) path=project->manifest;
    char* target=xidePathResolve(NULL,path); char* base=xidePathDirectory(target);
    xvalue* root=base ? projectJson(project,base) : NULL;
    int ok=root && unchangedManifest(target,project->manifest,project->source,error) && writeManifest(target,root,error);
    free(base);
    if(!ok) { free(target); xrtValueRelease(root); if(error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot allocate project serialization."); return 0; }
    free(project->manifest); project->manifest=target; xrtValueRelease(project->source); project->source=root; project->dirty=0;
    return 1;
}
int xideWorkspaceSave(xide_workspace* workspace,const char* path,xide_model_error* error)
{
    success(error); if(!workspace || !workspace->source) return fail(error,XIDE_MODEL_INVALID,"No workspace to save.");
    if(!path) path=workspace->manifest;
    char *target=xidePathResolve(NULL,path),*base=xidePathDirectory(target);
    xvalue *root=xrtValueClone(workspace->source),*projects=xrtValueArray();
    int ok=target && base && root && projects && setString(root,"format","xlang-workspace-1") && setString(root,"name",workspace->name) &&
        setString(root,"startupProject",workspace->startupProject) && xrtValueObjectSetNew(root,view("files"),filesJson(&workspace->files,base,0));
    for(size_t i=0;ok && i<workspace->projectCount;++i) {
        const xide_project* project=&workspace->projects[i];
        xvalue* item=xrtValueClone(project->referenceSource); char* relative=xidePathRelative(base,project->manifest);
        ok=item && relative && setString(item,"id",project->id) && setString(item,"path",relative); free(relative);
        if(ok) ok=xrtValueArrayAppendNew(projects,item); else xrtValueRelease(item);
    }
    if(ok) ok=xrtValueObjectSetNew(root,view("projects"),projects); else xrtValueRelease(projects);
    if(ok) ok=unchangedManifest(target,workspace->manifest,workspace->source,error) && writeManifest(target,root,error);
    free(base);
    if(!ok) { free(target); xrtValueRelease(root); if(error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot allocate workspace serialization."); return 0; }
    free(workspace->manifest); workspace->manifest=target; xrtValueRelease(workspace->source); workspace->source=root; workspace->dirty=0;
    return 1;
}
int xideWorkspaceAddProject(xide_workspace* workspace,const char* path,const char* id,xide_model_error* error)
{
    success(error); if(!workspace || !workspace->source || !path || (id && !*id)) return fail(error,XIDE_MODEL_INVALID,"Invalid project reference.");
    char* base=workspace->manifest ? xidePathDirectory(workspace->manifest) : NULL;
    char* absolutePath=xidePathResolve(base,path); free(base);
    if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid project path.");
    int ok=addReference(workspace,id,absolutePath,NULL,0,1,error); free(absolutePath);
    if(ok) workspace->dirty=1;
    return ok;
}
int xideWorkspaceRemoveProject(xide_workspace* workspace,const char* id,xide_model_error* error)
{
    success(error); if(!workspace || !id) return fail(error,XIDE_MODEL_INVALID,"Invalid project ID.");
    for(size_t i=0;i<workspace->projectCount;++i) if(!strcmp(workspace->projects[i].id,id)) {
        if(!strcmp(workspace->startupProject,id)) workspace->startupProject[0]=0;
        xideProjectClear(&workspace->projects[i]);
        memmove(&workspace->projects[i],&workspace->projects[i+1],(workspace->projectCount-i-1)*sizeof(*workspace->projects));
        --workspace->projectCount; workspace->dirty=1; return 1;
    }
    return fail(error,XIDE_MODEL_INVALID,"Project ID does not exist.");
}
int xideWorkspaceSetStartup(xide_workspace* workspace,const char* id,xide_model_error* error)
{
    success(error); if(!workspace || !workspace->source) return fail(error,XIDE_MODEL_INVALID,"No workspace.");
    if(!id) id="";
    xide_project* project=*id ? xideWorkspaceFindProject(workspace,id) : NULL;
    if(*id && (!project || !project->available)) return fail(error,XIDE_MODEL_INVALID,"Startup project must be available.");
    if(!strcmp(workspace->startupProject,id)) return 1;
    char* selected=copy(id); if(!selected) return fail(error,XIDE_MODEL_MEMORY,"Cannot allocate startup project ID.");
    free(workspace->startupProject); workspace->startupProject=selected; workspace->dirty=1; return 1;
}
int xideProjectCreate(xide_project* project,const char* manifest,const char* name,const char* entry,xide_model_error* error)
{
    success(error); if(!project || !manifest || !name || !*name || !entry || !xidePathHasExtension(entry,".xl")) return fail(error,XIDE_MODEL_INVALID,"Project requires a name, manifest and .xl entry.");
    xide_project temporary={0}; temporary.manifest=xidePathResolve(NULL,manifest);
    char* base=xidePathDirectory(temporary.manifest); temporary.entry=base ? xidePathResolve(base,entry) : NULL;
    temporary.name=copy(name); temporary.description=copy(""); temporary.activeConfiguration=copy("development"); temporary.kind=copy("exe"); temporary.source=xrtValueObject(); temporary.libraries=xrtValueArray(); temporary.available=temporary.dirty=1;
    int ok=temporary.manifest && temporary.entry && temporary.name && temporary.description && temporary.activeConfiguration && temporary.kind && temporary.source && temporary.libraries && ensureFile(&temporary.files,temporary.entry,"xlang",error);
    xide_configuration config={0};
    if(ok) {
        config.id=copy("development"); config.name=copy("开发配置"); config.output=xidePathResolve(base,"build/development/app.exe"); config.workingDirectory=copy(base);
        config.environment=xrtValueObject(); config.source=xrtValueObject(); config.runSource=xrtValueObject();
        ok=config.id && config.name && config.output && config.workingDirectory && config.environment && config.source && config.runSource &&
            reserve((void**)&temporary.configurations,&temporary.configurationCapacity,1,sizeof(config));
        if(ok) temporary.configurations[temporary.configurationCount++]=config; else configurationClear(&config);
    }
    free(base);
    if(!ok) { xideProjectClear(&temporary); if(error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot create project."); return 0; }
    xideProjectClear(project); *project=temporary; return 1;
}
int xideProjectSetMetadata(xide_project* project,const char* name,const char* description,const char* kind,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !name || !*name || !description || !kind || (strcmp(kind,"exe") && strcmp(kind,"dll") && strcmp(kind,"shared"))) return fail(error,XIDE_MODEL_INVALID,"Invalid project properties.");
    if(!strcmp(project->name,name) && !strcmp(project->description,description) && !strcmp(project->kind,kind)) return 1;
    char *n=copy(name),*d=copy(description),*k=copy(kind);
    if(!n || !d || !k) { free(n); free(d); free(k); return fail(error,XIDE_MODEL_MEMORY,"Cannot update properties."); }
    free(project->name); free(project->description); free(project->kind); project->name=n; project->description=d; project->kind=k; project->dirty=1; return 1;
}
int xideProjectSetEntry(xide_project* project,const char* path,xide_model_error* error)
{
    success(error); if(!project || !project->available || !path || !xidePathHasExtension(path,".xl")) return fail(error,XIDE_MODEL_INVALID,"Entry must be an .xl file.");
    char *base=xidePathDirectory(project->manifest),*absolutePath=xidePathResolve(base,path); free(base);
    if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid entry path.");
    if(xidePathEqual(project->entry,absolutePath)) { free(absolutePath); return 1; }
    if(!ensureFile(&project->files,absolutePath,"xlang",error)) { free(absolutePath); return 0; }
    free(project->entry); project->entry=absolutePath; project->dirty=1; return 1;
}
int xideProjectAddFile(xide_project* project,const char* path,const char* kind,xide_model_error* error)
{
    success(error); if(!project || !project->available || !path) return fail(error,XIDE_MODEL_INVALID,"No available project.");
    char *base=xidePathDirectory(project->manifest),*absolutePath=xidePathResolve(base,path); free(base);
    if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid file path.");
    if(!kind) kind=fileKind(absolutePath);
    if(!strcmp(kind,"c") && !xidePathHasExtension(absolutePath,".c")) { free(absolutePath); return fail(error,XIDE_MODEL_INVALID,"C source must have .c extension."); }
    if(!strcmp(kind,"resource-config") && !xidePathHasExtension(absolutePath,".json") && !xidePathHasExtension(absolutePath,".xlon")) { free(absolutePath); return fail(error,XIDE_MODEL_INVALID,"Resource configuration must be .json or .xlon."); }
    int ok=filesAdd(&project->files,NULL,absolutePath,kind,NULL,error);
    xide_strings* buildList=!strcmp(kind,"c") ? &project->cSources : !strcmp(kind,"resource-config") ? &project->resources : NULL;
    if(ok && buildList && !stringsAdd(buildList,absolutePath)) {
        fileClear(&project->files.items[--project->files.count]); ok=fail(error,XIDE_MODEL_MEMORY,"Cannot add compiler input.");
    }
    free(absolutePath); if(ok) project->dirty=1; return ok;
}
static int filesRemove(xide_files* files,const char* id,xide_model_error* error)
{
    for(size_t i=0;i<files->count;++i) if(!strcmp(files->items[i].id,id)) {
        fileClear(&files->items[i]); memmove(&files->items[i],&files->items[i+1],(files->count-i-1)*sizeof(*files->items)); --files->count; return 1;
    }
    return fail(error,XIDE_MODEL_INVALID,"File ID does not exist.");
}
int xideProjectRemoveFile(xide_project* project,const char* id,xide_model_error* error)
{
    success(error); if(!project || !project->available || !id) return fail(error,XIDE_MODEL_INVALID,"Invalid file reference.");
    for(size_t i=0;i<project->files.count;++i) if(!strcmp(project->files.items[i].id,id)) {
        const char* path=project->files.items[i].path;
        if(xidePathEqual(project->entry,path)) return fail(error,XIDE_MODEL_CONFLICT,"Choose a new entry before removing the current entry.");
        stringsRemovePath(&project->cSources,path); stringsRemovePath(&project->resources,path);
        int ok=filesRemove(&project->files,id,error); if(ok) project->dirty=1; return ok;
    }
    return fail(error,XIDE_MODEL_INVALID,"File ID does not exist.");
}
int xideWorkspaceAddFile(xide_workspace* workspace,const char* path,xide_model_error* error)
{
    success(error); if(!workspace || !workspace->source || !path) return fail(error,XIDE_MODEL_INVALID,"No workspace.");
    char *base=workspace->manifest ? xidePathDirectory(workspace->manifest) : NULL,*absolutePath=xidePathResolve(base,path); free(base);
    if(!absolutePath) return fail(error,XIDE_MODEL_INVALID,"Invalid workspace file path.");
    int ok=filesAdd(&workspace->files,NULL,absolutePath,NULL,NULL,error); free(absolutePath);
    if(ok) workspace->dirty=1;
    return ok;
}
int xideWorkspaceRemoveFile(xide_workspace* workspace,const char* id,xide_model_error* error)
{
    success(error); if(!workspace || !id) return fail(error,XIDE_MODEL_INVALID,"Invalid file ID.");
    int ok=filesRemove(&workspace->files,id,error); if(ok) workspace->dirty=1;
    return ok;
}
int xideProjectApplyProperties(xide_project* project,const char* name,const char* description,const char* kind,const char* entry,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !name || !*name || !description || !kind ||
       (strcmp(kind,"exe") && strcmp(kind,"dll") && strcmp(kind,"shared")) || !entry || !xidePathHasExtension(entry,".xl"))
        return fail(error,XIDE_MODEL_INVALID,"Name, output kind and .xl entry must be valid.");
    char *base=xidePathDirectory(project->manifest),*path=xidePathResolve(base,entry),*n=copy(name),*d=copy(description),*k=copy(kind); free(base);
    if(!path || !n || !d || !k) { free(path); free(n); free(d); free(k); return fail(error,XIDE_MODEL_MEMORY,"Cannot apply project properties."); }
    if(!ensureFile(&project->files,path,"xlang",error)) { free(path); free(n); free(d); free(k); return 0; }
    int changed=strcmp(project->name,name) || strcmp(project->description,description) || strcmp(project->kind,kind) || !xidePathEqual(project->entry,path);
    free(project->name); free(project->description); free(project->kind); free(project->entry);
    project->name=n; project->description=d; project->kind=k; project->entry=path;
    if(changed) project->dirty=1;
    return 1;
}
xide_configuration* xideProjectFindConfiguration(xide_project* project,const char* id)
{
    if(!project || !id) return NULL;
    for(size_t i=0;i<project->configurationCount;++i) if(!strcmp(project->configurations[i].id,id)) return &project->configurations[i];
    return NULL;
}
xvalue* xideProjectConfigurationsJson(const xide_project* project)
{
    if(!project || !project->available) return NULL;
    char* base=xidePathDirectory(project->manifest); xvalue* array=xrtValueArray(); int ok=base && array;
    for(size_t i=0;ok && i<project->configurationCount;++i) ok=xrtValueArrayAppendNew(array,configurationJson(&project->configurations[i],base));
    free(base); if(!ok) { xrtValueRelease(array); return NULL; } return array;
}
int xideProjectApplyConfigurations(xide_project* project,const xvalue* array,const char* active,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !isArray(array) || !active) return fail(error,XIDE_MODEL_INVALID,"Invalid configurations.");
    xide_project temporary={0}; char *base=xidePathDirectory(project->manifest),*selected=copy(active); int ok=base && selected;
    for(size_t i=0;ok && i<xrtValueCount(array);++i) {
        xide_configuration config={0}; ok=readConfiguration(xrtValueArrayGet(array,i),base,&config,error);
        if(ok && xideProjectFindConfiguration(&temporary,config.id)) ok=fail(error,XIDE_MODEL_CONFLICT,"Duplicate configuration ID: %s",config.id);
        if(ok) ok=reserve((void**)&temporary.configurations,&temporary.configurationCapacity,temporary.configurationCount+1,sizeof(config));
        if(ok) temporary.configurations[temporary.configurationCount++]=config; else configurationClear(&config);
    }
    if(ok && ((*active && !xideProjectFindConfiguration(&temporary,active)) || (!*active && temporary.configurationCount))) ok=fail(error,XIDE_MODEL_INVALID,"Choose an existing active configuration.");
    free(base);
    if(!ok) { free(selected); xideProjectClear(&temporary); if(error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot allocate configurations."); return 0; }
    xvalue *before=xideProjectConfigurationsJson(project),*after=xideProjectConfigurationsJson(&(xide_project){.available=1,.manifest=project->manifest,.configurations=temporary.configurations,.configurationCount=temporary.configurationCount});
    int same=before && after && xrtValueEqual(before,after) && !strcmp(project->activeConfiguration ? project->activeConfiguration : "",active);
    xrtValueRelease(before); xrtValueRelease(after);
    if(same) { free(selected); xideProjectClear(&temporary); return 1; }
    for(size_t i=0;i<project->configurationCount;++i) configurationClear(&project->configurations[i]);
    free(project->configurations); free(project->activeConfiguration);
    project->configurations=temporary.configurations; project->configurationCount=temporary.configurationCount; project->configurationCapacity=temporary.configurationCapacity;
    project->activeConfiguration=selected; project->dirty=1; return 1;
}
int xideProjectSetActiveConfiguration(xide_project* project,const char* id,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !id || !xideProjectFindConfiguration(project,id)) return fail(error,XIDE_MODEL_INVALID,"Configuration ID does not exist.");
    if(project->activeConfiguration && !strcmp(project->activeConfiguration,id)) return 1;
    char* selected=copy(id); if(!selected) return fail(error,XIDE_MODEL_MEMORY,"Cannot allocate configuration ID.");
    free(project->activeConfiguration); project->activeConfiguration=selected; project->dirty=1; return 1;
}

xvalue* xideProjectReferencesJson(const xide_project* project)
{
    if(!project || !project->available) return NULL;
    char* base=xidePathDirectory(project->manifest); xvalue* result=xrtValueObject();
    int okay=base && result && xrtValueObjectSetNew(result,view("files"),filesJson(&project->files,base,1)) &&
        xrtValueObjectSetNew(result,view("libraries"),xrtValueDeepClone(project->libraries));
    free(base); if(!okay) { xrtValueRelease(result); return NULL; } return result;
}
int xideProjectApplyReferences(xide_project* project,const xvalue* references,xide_model_error* error)
{
    success(error);
    if(!project || !project->available || !isObject(references) || !isArray(get(references,"files")) || !isArray(get(references,"libraries")))
        return fail(error,XIDE_MODEL_INVALID,"Expected project file and library arrays.");
    xide_project next={0}; char* base=xidePathDirectory(project->manifest);
    int okay=base && readFiles(references,base,&next.files,error),entry=0;
    for(size_t i=0;okay && i<next.files.count;++i) {
        xide_file* f=&next.files.items[i];
        if(xidePathEqual(f->path,project->entry)) {
            entry=1; if(strcmp(f->kind,"xlang")) okay=fail(error,XIDE_MODEL_INVALID,"The entry reference must keep its xlang kind.");
        }
        const char* ext=!strcmp(f->kind,"c") ? ".c" : !strcmp(f->kind,"xlang") ? ".xl" : !strcmp(f->kind,"header") ? ".h" : NULL;
        if(okay && ext && !xidePathHasExtension(f->path,ext)) okay=fail(error,XIDE_MODEL_INVALID,"Wrong extension for file kind: %s",f->kind);
        if(okay && !strcmp(f->kind,"resource-config") && !xidePathHasExtension(f->path,".json") && !xidePathHasExtension(f->path,".xlon"))
            okay=fail(error,XIDE_MODEL_INVALID,"Resource configurations must use .json or .xlon.");
        if(okay && (!strcmp(f->kind,"c") || !strcmp(f->kind,"resource-config")))
            okay=stringsAdd(!strcmp(f->kind,"c") ? &next.cSources : &next.resources,f->path);
    }
    if(okay && !entry) okay=fail(error,XIDE_MODEL_INVALID,"Keep the entry reference; change the entry in project properties first.");
    const xvalue* libs=get(references,"libraries");
    for(size_t i=0;okay && i<xrtValueCount(libs);++i) {
        const xvalue* item=xrtValueArrayGet(libs,i); char *kind=NULL,*name=NULL,*id=NULL;
        okay=isObject(item) && readString(item,"kind",NULL,1,&kind,error) && readString(item,"name",NULL,1,&name,error);
        if(okay && get(item,"id")) okay=readString(item,"id",NULL,1,&id,error);
        for(size_t j=0;okay && j<i;++j) {
            const xvalue* prior=xrtValueArrayGet(libs,j); char *pk=stringValue(get(prior,"kind")),*pn=stringValue(get(prior,"name")),*pi=stringValue(get(prior,"id"));
            if((pk && pn && !strcmp(pk,kind) && !strcmp(pn,name)) || (id && pi && !strcmp(id,pi)))
                okay=fail(error,XIDE_MODEL_CONFLICT,"Duplicate library identity or kind/name.");
            free(pk); free(pn); free(pi);
        }
        free(kind); free(name); free(id);
        if(!okay && error && !error->code) fail(error,XIDE_MODEL_INVALID,"Invalid library reference.");
    }
    if(okay) { next.libraries=xrtValueDeepClone(libs); okay=next.libraries!=NULL; }
    free(base);
    if(!okay) { xideProjectClear(&next); if(error && !error->code) fail(error,XIDE_MODEL_MEMORY,"Cannot allocate project references."); return 0; }
    next.available=1; next.manifest=copy(project->manifest);
    xvalue *before=xideProjectReferencesJson(project),*after=next.manifest ? xideProjectReferencesJson(&next) : NULL;
    if(!before || !after) { xrtValueRelease(before); xrtValueRelease(after); xideProjectClear(&next); return fail(error,XIDE_MODEL_MEMORY,"Cannot compare project references."); }
    int same=xrtValueEqual(before,after); xrtValueRelease(before); xrtValueRelease(after);
    if(same) { xideProjectClear(&next); return 1; }
    filesClear(&project->files); stringsClear(&project->cSources); stringsClear(&project->resources); xrtValueRelease(project->libraries);
    project->files=next.files; project->cSources=next.cSources; project->resources=next.resources; project->libraries=next.libraries; project->dirty=1;
    free(next.manifest); return 1;
}
