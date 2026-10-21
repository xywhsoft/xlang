#include "model/workspace.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expression) do { if(!(expression)) { fprintf(stderr,"model_test:%d: %s; model error: %s\n",__LINE__,#expression,error.message); failed=1; goto cleanup; } } while(0)
static char* join(const char* base,const char* relative) { return xidePathResolve(base,relative); }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static xvalue* field(xvalue* root,const char* key) { return xrtValueObjectGet(root,(xstrview){key,strlen(key)}); }
static int sameString(xvalue* value,const char* expected)
{
    xstrview text; return xrtValueGetString(value,&text) && text.Size==strlen(expected) && !memcmp(text.Data,expected,text.Size);
}
static wchar_t* wide(const char* text)
{
    int count=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0); wchar_t* value=malloc((size_t)count*sizeof(*value));
    if(value) MultiByteToWideChar(CP_UTF8,0,text,-1,value,count);
    return value;
}
static int writeValue(const char* path,const xvalue* value)
{
    size_t size=0; char* text=xrtJsonStringify(value,true,&size);
    int result=text && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,size}); xrtFree(text); return result;
}
int main(int argc,char** argv)
{
    int failed=0; xide_model_error error={0}; xide_project project={0},reload={0},generated={0}; xide_workspace workspace={0},other={0};
    xvalue* json=NULL; char *root=NULL,*projectPath=NULL,*workspacePath=NULL,*saveAs=NULL,*projectAs=NULL,*sourcePath=NULL,*nativePath=NULL,*aliasPath=NULL,*badPath=NULL,*backup=NULL,*relative=NULL;
    wchar_t* first=NULL; wchar_t* second=NULL; HANDLE locked=INVALID_HANDLE_VALUE; unsigned char* before=NULL;
    CHECK(argc==2); root=join(argv[1],"中文 空格工程"); CHECK(root && xrtDirCreateAll(root));
    char* sourceDir=join(root,"src"); CHECK(sourceDir && xrtDirCreateAll(sourceDir)); free(sourceDir);
    char* saveDir=join(root,"另存为"); CHECK(saveDir && xrtDirCreateAll(saveDir)); free(saveDir);
    projectPath=join(root,"app.xlp"); workspacePath=join(root,"workspace.xlw"); saveAs=join(root,"另存为/copy.xlw"); projectAs=join(root,"另存为/copy.xlp");
    sourcePath=join(root,"src/main.xl"); nativePath=join(root,"src/native.c"); char aliasName[80]; snprintf(aliasName,sizeof(aliasName),"src/alias-%llu.c",(unsigned long long)GetTickCount64()); aliasPath=join(root,aliasName); badPath=join(root,"bad.json"); backup=join(root,"locked.xlw");
    CHECK(projectPath && workspacePath && saveAs && projectAs && sourcePath && nativePath && aliasPath && badPath && backup);
    CHECK(writeText(sourcePath,"func main(...) as int\n    return 0\nend\n") && writeText(nativePath,"int helper(void) { return 1; }\n"));
    const char* projectJson="{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"src/main.xl\",\"cSources\":[\"src/native.c\"],\"resources\":[\"app.res.json\"],\"vendor\":{\"keep\":true},"
      "\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"示例工程\",\"description\":\"描述\",\"vendorIde\":7,\"files\":[{\"id\":\"main-source\",\"path\":\"src/main.xl\",\"kind\":\"xlang\",\"custom\":42}],"
      "\"libraries\":[{\"kind\":\"package\",\"name\":\"console\",\"keep\":true}],\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/app.exe\",\"includePaths\":[\"inc\",\"../shared\"],\"libraryPaths\":[\"lib\"],\"linkLibraries\":[\"user32\"],\"vendorConfig\":9,"
      "\"run\":{\"workingDirectory\":\".\",\"arguments\":[\"中文参数\",\"a b\"],\"environment\":{\"HELLO\":\"世界\"},\"vendorRun\":11}}]}}";
    CHECK(writeText(projectPath,projectJson));
    CHECK(xideProjectLoad(&project,projectPath,&error));
    CHECK(project.available && !project.dirty && !strcmp(project.name,"示例工程") && project.files.count==3);
    CHECK(project.configurationCount==1 && project.configurations[0].includePaths.count==2 && project.configurations[0].arguments.count==2);
    CHECK(xidePathEqual(project.entry,sourcePath) && !strcmp(project.files.items[0].id,"main-source"));
    relative=xidePathRelative(root,sourcePath); CHECK(relative && !strcmp(relative,"src/main.xl")); free(relative); relative=NULL;
    relative=xidePathRelative(root,root); CHECK(relative && !strcmp(relative,".")); free(relative); relative=NULL;
    first=wide(nativePath); second=wide(aliasPath); CHECK(first && second);
    /* Existing fixture reruns keep the previous hard link; the file identity remains equal. */
    CHECK(CreateHardLinkW(second,first,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    CHECK(xidePathEqual(nativePath,aliasPath));
    CHECK(!xideProjectAddFile(&project,aliasPath,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && project.files.count==3);
    CHECK(!xideProjectRemoveFile(&project,"main-source",&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xideProjectSetMetadata(&project,"新名称","新的描述","dll",&error));
    CHECK(xideProjectAddFile(&project,"src/new.c",NULL,&error) && project.cSources.count==2);
    char* newId=malloc(strlen(project.files.items[project.files.count-1].id)+1); CHECK(newId); strcpy(newId,project.files.items[project.files.count-1].id);
    CHECK(xideProjectRemoveFile(&project,newId,&error) && project.cSources.count==1); free(newId);
    CHECK(xideProjectSetEntry(&project,"src/other.xl",&error));
    CHECK(xideProjectSave(&project,projectAs,&error) && !project.dirty);
    CHECK(xideProjectLoad(&reload,projectAs,&error) && xidePathEqual(reload.entry,project.entry) && xidePathEqual(reload.cSources.items[0],nativePath));
    CHECK(!strcmp(reload.name,"新名称") && !strcmp(reload.kind,"dll") && reload.files.count==4);
    CHECK(xidePathEqual(reload.configurations[0].output,project.configurations[0].output) && xidePathEqual(reload.configurations[0].workingDirectory,root));
    json=xrtJsonParseFile(projectAs); CHECK(json && field(json,"vendor"));
    xvalue* ide=field(json,"ide"); CHECK(field(ide,"vendorIde"));
    CHECK(field(xrtValueArrayGet(field(ide,"files"),0),"custom"));
    xvalue* config=xrtValueArrayGet(field(ide,"configurations"),0); CHECK(field(config,"vendorConfig") && field(field(config,"run"),"vendorRun"));
    CHECK(sameString(field(field(field(config,"run"),"environment"),"HELLO"),"世界"));
    xrtValueRelease(json); json=NULL;
    CHECK(xideWorkspaceCreate(&workspace,"测试工作空间",&error));
    CHECK(xideWorkspaceAddProject(&workspace,projectPath,"app",&error));
    CHECK(xideWorkspaceAddProject(&workspace,projectAs,"copy",&error));
    CHECK(xideWorkspaceSetStartup(&workspace,"app",&error));
    CHECK(xideWorkspaceFindProject(&workspace,"copy") && !strcmp(workspace.startupProject,"app"));
    CHECK(!xideWorkspaceAddProject(&workspace,projectPath,"duplicate",&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xideWorkspaceAddFile(&workspace,sourcePath,&error));
    CHECK(xideWorkspaceSave(&workspace,workspacePath,&error));
    CHECK(xideWorkspaceSave(&workspace,saveAs,&error));
    CHECK(xideWorkspaceLoad(&other,saveAs,&error) && other.projectCount==2 && !strcmp(other.startupProject,"app"));
    CHECK(xidePathEqual(other.projects[0].manifest,projectPath) && xidePathEqual(other.files.items[0].path,sourcePath));
    CHECK(xideWorkspaceRemoveProject(&other,"app",&error) && !*other.startupProject && other.projectCount==1);
    CHECK(xidePathExists(projectPath)); /* Remove is reference-only. */
    const char* missing="{\"format\":\"xlang-workspace-1\",\"name\":\"缺失工程\",\"startupProject\":\"missing\",\"vendor\":{\"keep\":1},\"projects\":[{\"id\":\"missing\",\"path\":\"lost.xlp\",\"vendorRef\":9}],\"files\":[{\"id\":\"note\",\"path\":\"note.txt\",\"vendorFile\":8}]}";
    CHECK(writeText(badPath,missing) && xideWorkspaceLoad(&other,badPath,&error));
    CHECK(other.projectCount==1 && !other.projects[0].available && other.projects[0].issue && !strcmp(other.startupProject,"missing"));
    CHECK(xideWorkspaceSave(&other,saveAs,&error));
    json=xrtJsonParseFile(saveAs); CHECK(json && field(json,"vendor") && field(xrtValueArrayGet(field(json,"projects"),0),"vendorRef") && field(xrtValueArrayGet(field(json,"files"),0),"vendorFile"));
    xrtValueRelease(json); json=NULL;
    const char* invalid[]={"{", "[]", "{\"format\":\"xlang-workspace-1\",\"name\":\"invalid\",\"startupProject\":\"unknown\"}",
      "{\"format\":\"xlang-workspace-1\",\"name\":\"invalid\",\"projects\":[{\"id\":\"x\",\"path\":\"a.xlp\"},{\"id\":\"x\",\"path\":\"b.xlp\"}]}",
      "{\"format\":\"xlang-workspace-1\",\"name\":\"invalid\\u0000name\"}"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        CHECK(writeText(badPath,invalid[i])); CHECK(!xideWorkspaceLoad(&other,badPath,&error));
        CHECK(other.projectCount==1 && !strcmp(other.name,"缺失工程")); /* Failed loads are transactional. */
    }
    CHECK(writeText(badPath,"{\"Version\":1,\"Name\":\"old\",\"Projects\":[]}"));
    CHECK(!xideWorkspaceLoad(&other,badPath,&error) && error.code==XIDE_MODEL_LEGACY);
    CHECK(writeText(badPath,"{\"format\":\"xlang-project-1\",\"entry\":\"main.c\"}"));
    CHECK(!xideProjectLoad(&reload,badPath,&error) && reload.available && !strcmp(reload.name,"新名称"));
    CHECK(xideWorkspaceSave(&workspace,backup,&error));
    free(first); first=wide(backup); CHECK(first);
    locked=CreateFileW(first,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    size_t beforeSize=0; before=xrtFileReadAllLimit(backup,100000,&beforeSize); CHECK(before);
    CHECK(xideWorkspaceSetStartup(&workspace,"copy",&error));
    CHECK(!xideWorkspaceSave(&workspace,backup,&error) && error.code==XIDE_MODEL_IO && workspace.dirty);
    size_t afterSize=0; unsigned char* after=xrtFileReadAllLimit(backup,100000,&afterSize); CHECK(after && beforeSize==afterSize && !memcmp(before,after,beforeSize)); xrtFree(after);
    CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(xideWorkspaceSave(&workspace,backup,&error) && !workspace.dirty);
    CHECK(writeText(backup,"{\"format\":\"xlang-workspace-1\",\"name\":\"外部工作空间\"}"));
    CHECK(xideWorkspaceSetStartup(&workspace,"app",&error) && !xideWorkspaceSave(&workspace,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && workspace.dirty);
    CHECK(writeValue(backup,workspace.source) && xideWorkspaceSave(&workspace,NULL,&error));
    const char* badProjects[]={
      "{\"format\":\"xlang-project-1\",\"entry\":\"src/main.xl\",\"ide\":{\"libraries\":[7]}}",
      "{\"format\":\"xlang-project-1\",\"entry\":\"src/main.xl\",\"ide\":{\"libraries\":[{\"kind\":\"package\",\"name\":\"bad\\u0000name\"}]}}",
      "{\"format\":\"xlang-project-1\",\"entry\":\"src/main.xl\",\"ide\":{\"configurations\":[{\"id\":\"dev\",\"run\":{\"environment\":{\"KEY\":3}}}]}}"
    };
    for(size_t i=0;i<sizeof(badProjects)/sizeof(*badProjects);++i) CHECK(writeText(badPath,badProjects[i]) && !xideProjectLoad(&reload,badPath,&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(xideProjectCreate(&generated,projectPath,"动态容量","src/main.xl",&error));
    for(int i=0;i<600;++i) { char path[80]; snprintf(path,sizeof(path),"src/file-%d.xl",i); CHECK(xideProjectAddFile(&generated,path,NULL,&error)); }
    CHECK(generated.files.count==601 && xideProjectSave(&generated,projectPath,&error));
    CHECK(xideProjectLoad(&reload,projectPath,&error) && reload.files.count==601);
    CHECK(writeText(projectPath,"{\"format\":\"xlang-project-1\",\"entry\":\"src/main.xl\"}"));
    CHECK(xideProjectSetMetadata(&reload,"动态容量","外部冲突已验证","exe",&error) && !xideProjectSave(&reload,NULL,&error) && error.code==XIDE_MODEL_CONFLICT && reload.dirty);
    CHECK(writeValue(projectPath,reload.source) && xideProjectSave(&reload,NULL,&error));
    CHECK(xideWorkspaceCreate(&other,"40 个项目",&error));
    for(int i=0;i<40;++i) {
        char filename[80],id[80]; snprintf(filename,sizeof(filename),"project-%d.xlp",i); snprintf(id,sizeof(id),"project-%d",i);
        char* path=join(root,filename); CHECK(path && writeText(path,"{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"src/main.xl\"}"));
        CHECK(xideWorkspaceAddProject(&other,path,id,&error)); free(path);
    }
    CHECK(other.projectCount==40 && xideWorkspaceSave(&other,workspacePath,&error));
    CHECK(xideWorkspaceLoad(&workspace,workspacePath,&error) && workspace.projectCount==40);
    /* Snapshot decoding borrows JSON, never opens manifest/project bodies. */
    json=xrtJsonParse((xstrview){projectJson,strlen(projectJson)}); CHECK(json);
    CHECK(xideProjectDecode(&reload,badPath,json,&error) && reload.available && !strcmp(reload.name,"示例工程") && field(reload.source,"vendor"));
    xrtValueRelease(json); json=NULL;
    CHECK(xideProjectSetMetadata(&reload,"Export draft","unsaved","dll",&error));
    json=xideProjectExport(&reload,&error); CHECK(json && sameString(field(field(json,"ide"),"name"),"Export draft") && sameString(field(field(reload.source,"ide"),"name"),"示例工程") && field(json,"vendor")); xrtValueRelease(json); json=NULL;
    CHECK(xideProjectSetMetadata(&reload,"示例工程","描述","exe",&error));
    json=xrtValueArray(); CHECK(json && !xideProjectDecode(&reload,badPath,json,&error) && !strcmp(reload.name,"示例工程")); xrtValueRelease(json); json=NULL;
    json=xrtValueClone(workspace.source); CHECK(json && xideWorkspaceDecode(&other,workspacePath,json,&error));
    CHECK(other.projectCount==40 && !other.projects[0].available && strstr(other.projects[0].issue,"not been read"));
    CHECK(xrtValueEqual(other.source,json)); xrtValueRelease(json); json=NULL;
    json=xrtValueObject(); CHECK(json && !xideWorkspaceDecode(&other,workspacePath,json,&error) && other.projectCount==40); xrtValueRelease(json); json=NULL;
    puts("model_test passed: compiler-compatible projects, UTF-8 paths, hard-link deduplication, unknown-field round trips, save-as rebasing, missing projects, transactional loads/saves, 40 projects and 601 files; no GUI context");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xrtFree(before); xrtValueRelease(json); free(first); free(second);
    xideProjectClear(&project); xideProjectClear(&reload); xideProjectClear(&generated); xideWorkspaceClear(&workspace); xideWorkspaceClear(&other);
    free(root); free(projectPath); free(workspacePath); free(saveAs); free(projectAs); free(sourcePath); free(nativePath); free(aliasPath); free(badPath); free(backup); free(relative);
    return failed;
}