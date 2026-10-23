#include "services/analysis_context.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"analysis_project_context_test:%d: %s; %s\n",__LINE__,#call,error.message); return 1; } } while(0)
static const char* projectJson="{\"format\":\"xlang-project-1\",\"entry\":\"src/main.xl\",\"cSources\":[\"src/native.c\"],\"resources\":[\"data.res.json\"],"
    "\"ide\":{\"name\":\"工程\",\"files\":[{\"id\":\"other\",\"path\":\"src/other.xl\"}],\"activeConfiguration\":\"dev\",\"configurations\":["
    "{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/app.exe\",\"includePaths\":[\"inc first\",\"inc second\"],\"libraryPaths\":[\"lib\"],\"linkLibraries\":[\"user32\"],"
    "\"cAnalysis\":{\"systemIncludes\":[\"sys\"],\"forcedIncludes\":[\"forced.h\"],\"defines\":[\"ANSWER=42\"],\"undefines\":[\"OLD\"]},"
    "\"run\":{\"workingDirectory\":\"run\",\"arguments\":[\"one\"],\"environment\":{\"XLANG_PACKAGE_PATH\":\"run-only\"}}},"
    "{\"id\":\"release\",\"name\":\"发布\",\"output\":\"build/release.exe\"}]}}";
int main(int argc,char** argv)
{
    xide_model_error error={0}; xide_project project={0},bare={0};
    CHECK(argc==2);
    char* path=xidePathResolve(argv[1],"中文 工程/app.xlp"); CHECK(path);
    xvalue* json=xrtJsonParse((xstrview){projectJson,strlen(projectJson)});
    CHECK(json && xideProjectDecode(&project,path,json,&error)); xrtValueRelease(json);
    CHECK(project.configurationCount==2 && project.files.count==4);
    wchar_t cwdBefore[32768],cwdAfter[32768]; DWORD cwd=GetCurrentDirectoryW(32768,cwdBefore); CHECK(cwd && cwd<32768);
    char envBefore[32768],envAfter[32768]; SetLastError(ERROR_SUCCESS);
    DWORD env=GetEnvironmentVariableA("XLANG_PACKAGE_PATH",envBefore,sizeof(envBefore)); DWORD envError=GetLastError();
    xls_context* a=xideAnalysisProjectContext(&project,NULL,"native-profile-sha","captured-build-package",&error); CHECK(a && !error.code);
    const xls_context_spec* v=xlsContextSpec(a); CHECK(v && v->kind==XLS_CONTEXT_PROJECT && !strcmp(v->configuration,"dev"));
    CHECK(!strcmp(v->project,project.manifest) && !strcmp(v->entry,project.entry));
    char* directory=xidePathDirectory(project.manifest); CHECK(directory && !strcmp(v->directory,directory)); free(directory);
    CHECK(strcmp(v->directory,project.configurations[0].workingDirectory) && !strcmp(v->packagePath,"captured-build-package"));
    CHECK(v->lists[XLS_CONTEXT_MEMBERS].count==4 && v->lists[XLS_CONTEXT_C_SOURCES].count==1 && v->lists[XLS_CONTEXT_RESOURCES].count==1);
    for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) CHECK(v->lists[i].count>0);
    CHECK(!strcmp(v->lists[XLS_CONTEXT_C_DEFINES].items[0],"ANSWER=42") && !strcmp(v->lists[XLS_CONTEXT_C_UNDEFINES].items[0],"OLD"));
    CHECK(v->lists[XLS_CONTEXT_INCLUDES].count==2 && !strcmp(v->lists[XLS_CONTEXT_INCLUDES].items[0],project.configurations[0].includePaths.items[0]));
    xls_context* b=xideAnalysisProjectContext(&project,"dev","native-profile-sha","captured-build-package",&error); CHECK(b && xlsContextEqual(a,b));
    xls_context* c=xideAnalysisProjectContext(&project,"release","native-profile-sha","captured-build-package",&error); CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    CHECK(!xideAnalysisProjectContext(&project,"missing","native-profile-sha",NULL,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(!xideAnalysisProjectContext(&project,"","native-profile-sha",NULL,&error));
    CHECK(xideProjectSetActiveConfiguration(&project,"release",&error));
    c=xideAnalysisProjectContext(&project,NULL,"native-profile-sha","captured-build-package",&error); CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    c=xideAnalysisProjectContext(&project,"dev","other-profile","captured-build-package",&error); CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    c=xideAnalysisProjectContext(&project,"dev","native-profile-sha","other-package",&error); CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    /* Capture from the same decoded model after runtime-only changes. */
    xide_configuration* config=&project.configurations[0];
    config->name[0]='X'; config->output[0]='X'; config->workingDirectory[0]='X'; config->arguments.items[0][0]='X';
    project.name[0]='X'; project.description[0]=0;
    CHECK(xrtValueObjectSetNew(config->environment,(xstrview){"XLANG_PACKAGE_PATH",18},xrtValueString((xstrview){"changed-run-only",16})));
    c=xideAnalysisProjectContext(&project,"dev","native-profile-sha","captured-build-package",&error); CHECK(c && xlsContextEqual(a,c)); xlsContextRelease(c);
    /* Same file under another project is a separate semantic identity. */
    project.manifest[0]='X'; c=xideAnalysisProjectContext(&project,"dev","native-profile-sha","captured-build-package",&error);
    CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c); project.manifest[0]=path[0];
    const char* simple="{\"format\":\"xlang-project-1\",\"entry\":\"single.xl\"}";
    json=xrtJsonParse((xstrview){simple,strlen(simple)}); CHECK(json && xideProjectDecode(&bare,path,json,&error)); xrtValueRelease(json);
    c=xideAnalysisProjectContext(&bare,NULL,"native-profile-sha",NULL,&error); CHECK(c && !*xlsContextSpec(c)->configuration && !xlsContextSpec(c)->packagePath);
    xls_context* d=xideAnalysisProjectContext(&bare,"","native-profile-sha",NULL,&error); CHECK(d && xlsContextEqual(c,d)); xlsContextRelease(d);
    CHECK(!xideAnalysisProjectContext(&bare,"dev","native-profile-sha",NULL,&error));
    bare.available=0; CHECK(!xideAnalysisProjectContext(&bare,NULL,"native-profile-sha",NULL,&error)); bare.available=1;
    d=xideAnalysisFileContext("folder/../standalone.xl",argv[1],"native-profile-sha",NULL,&error); CHECK(d && xlsContextSpec(d)->kind==XLS_CONTEXT_XLANG);
    CHECK(!*xlsContextSpec(d)->project && !*xlsContextSpec(d)->configuration && xlsContextSpec(d)->lists[XLS_CONTEXT_MEMBERS].count==1);
    directory=xidePathDirectory(xlsContextSpec(d)->entry); CHECK(directory && !strcmp(directory,xlsContextSpec(d)->directory)); free(directory);
    xls_context* e=xideAnalysisFileContext("standalone.xl",argv[1],"native-profile-sha",NULL,&error); CHECK(e && xlsContextEqual(d,e)); xlsContextRelease(e);
    e=xideAnalysisFileContext("standalone.c",argv[1],"native-profile-sha","",&error); CHECK(e && xlsContextSpec(e)->kind==XLS_CONTEXT_C && xlsContextSpec(e)->packagePath && !*xlsContextSpec(e)->packagePath); xlsContextRelease(e);
    CHECK(!xideAnalysisFileContext("not-source.txt",argv[1],"native-profile-sha",NULL,&error));
    CHECK(!xideAnalysisFileContext(NULL,argv[1],"native-profile-sha",NULL,&error));
    CHECK(!xideAnalysisFileContext("standalone.xl",argv[1],"",NULL,&error));
    CHECK(GetCurrentDirectoryW(32768,cwdAfter)==cwd && !wcscmp(cwdBefore,cwdAfter));
    SetLastError(ERROR_SUCCESS); DWORD envNow=GetEnvironmentVariableA("XLANG_PACKAGE_PATH",envAfter,sizeof(envAfter)); DWORD envNowError=GetLastError();
    CHECK(env==envNow && (env ? !memcmp(envBefore,envAfter,env+1) : envError==envNowError));
    xideProjectClear(&project); xideProjectClear(&bare); free(path);
    CHECK(xlsContextEqual(a,b) && !strcmp(v->configuration,"dev") && !strcmp(v->lists[XLS_CONTEXT_C_DEFINES].items[0],"ANSWER=42"));
    CHECK(!*xlsContextSpec(c)->configuration && xlsContextSpec(d)->kind==XLS_CONTEXT_XLANG);
    xlsContextRelease(a); xlsContextRelease(b); xlsContextRelease(c); xlsContextRelease(d);
    printf("analysis_project_context_test passed: %d checks; existing model, configurations and standalone inputs\n",checks); return 0;
}
