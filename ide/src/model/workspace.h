#ifndef XLANG_IDE_WORKSPACE_MODEL_H
#define XLANG_IDE_WORKSPACE_MODEL_H
#include "app_xrt.h"
#include <stddef.h>
enum { XIDE_MODEL_INVALID=1, XIDE_MODEL_IO, XIDE_MODEL_MEMORY, XIDE_MODEL_CONFLICT, XIDE_MODEL_LEGACY };
typedef struct xide_model_error { int code; char message[512]; } xide_model_error;
typedef struct xide_strings { char** items; size_t count,capacity; } xide_strings;
typedef struct xide_file {
    char *id,*path,*kind;
    xvalue* source;
} xide_file;
typedef struct xide_files { xide_file* items; size_t count,capacity; } xide_files;
typedef struct xide_c_analysis_config {
    xide_strings systemIncludes,forcedIncludes,defines,undefines;
} xide_c_analysis_config;
typedef struct xide_test_config {
    char *program,*workingDirectory;
    xide_strings arguments;
    xvalue *environment,*source;
} xide_test_config;
typedef struct xide_configuration {
    char *id,*name,*output,*workingDirectory;
    xide_strings includePaths,libraryPaths,linkLibraries,arguments;
    xvalue *environment,*source,*runSource;
    xide_c_analysis_config cAnalysis;
    xide_test_config test;
} xide_configuration;
typedef struct xide_project {
    char *id,*manifest,*name,*description,*kind,*entry,*issue,*activeConfiguration;
    xide_strings cSources,resources;
    xide_files files;
    xide_configuration* configurations;
    size_t configurationCount,configurationCapacity;
    xvalue *source,*referenceSource,*libraries;
    int available,dirty;
} xide_project;
typedef struct xide_workspace {
    char *manifest,*name,*startupProject;
    xide_project* projects;
    size_t projectCount,projectCapacity;
    xide_files files;
    xvalue* source;
    int dirty;
} xide_workspace;
/* Object-like macro name, optionally =single-line UTF-8 replacement. */
int xideCAnalysisMacroValid(const char* value,int define);
void xideProjectClear(xide_project* project);
void xideWorkspaceClear(xide_workspace* workspace);
xide_project* xideWorkspaceFindProject(xide_workspace* workspace,const char* id);
int xideWorkspaceCreate(xide_workspace* workspace,const char* name,xide_model_error* error);
/* Transactional decoding of an owned background snapshot. Workspace decoding retains references without reading project bodies. */
int xideWorkspaceDecode(xide_workspace* workspace,const char* path,const xvalue* root,xide_model_error* error);
int xideWorkspaceLoad(xide_workspace* workspace,const char* path,xide_model_error* error);
xvalue* xideWorkspaceExport(const xide_workspace* workspace,xide_model_error* error);
int xideWorkspaceSave(xide_workspace* workspace,const char* path,xide_model_error* error);
int xideWorkspaceAddProject(xide_workspace* workspace,const char* path,const char* id,xide_model_error* error);
int xideWorkspaceRemoveProject(xide_workspace* workspace,const char* id,xide_model_error* error);
int xideWorkspaceSetStartup(xide_workspace* workspace,const char* id,xide_model_error* error);
int xideProjectCreate(xide_project* project,const char* manifest,const char* name,const char* entry,xide_model_error* error);
int xideProjectDecode(xide_project* project,const char* path,const xvalue* root,xide_model_error* error);
int xideProjectLoad(xide_project* project,const char* path,xide_model_error* error);
xvalue* xideProjectExport(const xide_project* project,xide_model_error* error);
int xideProjectSave(xide_project* project,const char* path,xide_model_error* error);
int xideProjectSetMetadata(xide_project* project,const char* name,const char* description,const char* kind,xide_model_error* error);
int xideProjectSetEntry(xide_project* project,const char* path,xide_model_error* error);
int xideProjectAddFile(xide_project* project,const char* path,const char* kind,xide_model_error* error);
int xideProjectRemoveFile(xide_project* project,const char* id,xide_model_error* error);
int xideWorkspaceAddFile(xide_workspace* workspace,const char* path,xide_model_error* error);
int xideWorkspaceRemoveFile(xide_workspace* workspace,const char* id,xide_model_error* error);
int xideProjectApplyProperties(xide_project* project,const char* name,const char* description,const char* kind,const char* entry,xide_model_error* error);
xide_configuration* xideProjectFindConfiguration(xide_project* project,const char* id);
xvalue* xideProjectConfigurationsJson(const xide_project* project);
int xideProjectApplyConfigurations(xide_project* project,const xvalue* array,const char* active,xide_model_error* error);
xvalue* xideProjectReferencesJson(const xide_project* project);
int xideProjectApplyReferences(xide_project* project,const xvalue* references,xide_model_error* error);
int xideProjectSetActiveConfiguration(xide_project* project,const char* id,xide_model_error* error);
#endif
