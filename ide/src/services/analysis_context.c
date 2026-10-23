#include "services/analysis_context.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xls_context* contextFail(xide_model_error* error,int code,const char* message)
{ if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return NULL; }
static xls_context* create(const xls_context_spec* spec,xide_model_error* error)
{
    enum xls_context_error status;
    xls_context* result=xlsContextCreate(spec,16u*1024u*1024u,&status);
    if(result) { if(error) memset(error,0,sizeof(*error)); return result; }
    return contextFail(error,status==XLS_CONTEXT_MEMORY ? XIDE_MODEL_MEMORY : XIDE_MODEL_INVALID,
        status==XLS_CONTEXT_LIMIT ? "Analysis context capacity exceeded." : "Invalid analysis context or unavailable memory.");
}
static xls_context_strings strings(const xide_strings* value)
{ return (xls_context_strings){(const char* const*)value->items,value->count}; }
xls_context* xideAnalysisProjectContext(const xide_project* project,const char* configuration,
    const char* profile,const char* packagePath,xide_model_error* error)
{
    if(!project || !project->available || !project->manifest || !project->entry ||
        (project->configurationCount && !project->configurations) || (project->files.count && !project->files.items))
        return contextFail(error,XIDE_MODEL_INVALID,"An available decoded project is required.");
    const char* id=configuration ? configuration : project->activeConfiguration;
    const xide_configuration* chosen=NULL;
    if(project->configurationCount) {
        if(id && *id) for(size_t i=0;i<project->configurationCount;++i)
            if(!strcmp(project->configurations[i].id,id)) { chosen=&project->configurations[i]; break; }
        if(!chosen) return contextFail(error,XIDE_MODEL_INVALID,"Analysis configuration does not exist.");
    } else if(id && *id) return contextFail(error,XIDE_MODEL_INVALID,"Implicit project configuration has no ID.");
    if(project->files.count>65536) return contextFail(error,XIDE_MODEL_INVALID,"Too many analysis members.");
    char* directory=xidePathDirectory(project->manifest);
    const char** members=project->files.count ? xrtMalloc(project->files.count*sizeof(*members)) : NULL;
    if(!directory || (project->files.count && !members)) {
        free(directory); xrtFree(members); return contextFail(error,XIDE_MODEL_MEMORY,"Cannot capture analysis context.");
    }
    for(size_t i=0;i<project->files.count;++i) members[i]=project->files.items[i].path;
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project->manifest,.configuration=chosen ? chosen->id : "",
        .entry=project->entry,.directory=directory,.analysisProfile=profile,.targetKind=project->kind,.packagePath=packagePath};
    spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,project->files.count};
    spec.lists[XLS_CONTEXT_C_SOURCES]=strings(&project->cSources);
    spec.lists[XLS_CONTEXT_RESOURCES]=strings(&project->resources);
    if(chosen) {
        spec.lists[XLS_CONTEXT_INCLUDES]=strings(&chosen->includePaths);
        spec.lists[XLS_CONTEXT_LIBRARY_PATHS]=strings(&chosen->libraryPaths);
        spec.lists[XLS_CONTEXT_LINK_LIBRARIES]=strings(&chosen->linkLibraries);
        spec.lists[XLS_CONTEXT_C_SYSTEM_INCLUDES]=strings(&chosen->cAnalysis.systemIncludes);
        spec.lists[XLS_CONTEXT_C_FORCED_INCLUDES]=strings(&chosen->cAnalysis.forcedIncludes);
        spec.lists[XLS_CONTEXT_C_DEFINES]=strings(&chosen->cAnalysis.defines);
        spec.lists[XLS_CONTEXT_C_UNDEFINES]=strings(&chosen->cAnalysis.undefines);
    }
    xls_context* result=create(&spec,error); xrtFree(members); free(directory); return result;
}
xls_context* xideAnalysisFileContext(const char* path,const char* base,
    const char* profile,const char* packagePath,xide_model_error* error)
{
    if(!path || !*path) return contextFail(error,XIDE_MODEL_INVALID,"A source path is required.");
    char* absolute=xidePathResolve(base,path); char* directory=absolute ? xidePathDirectory(absolute) : NULL;
    enum xls_context_kind kind=absolute && xidePathHasExtension(absolute,".xl") ? XLS_CONTEXT_XLANG : XLS_CONTEXT_C;
    if(!absolute || !directory || (kind==XLS_CONTEXT_C && !xidePathHasExtension(absolute,".c"))) {
        free(absolute); free(directory); return contextFail(error,XIDE_MODEL_INVALID,"Expected a standalone .xl or .c source.");
    }
    xls_context_spec spec={.kind=kind,.entry=absolute,.directory=directory,.analysisProfile=profile,
        .targetKind="exe",.packagePath=packagePath};
    const char* member=absolute;
    spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){&member,1};
    xls_context* result=create(&spec,error); free(absolute); free(directory); return result;
}
