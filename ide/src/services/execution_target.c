#include "services/execution_target.h"
#include "app/xide_app.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "services/analysis_configuration.h"
#include "platform/path.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0};return xrtValueGetString(v,&s) && s.Size<=8192 && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
static const char* modes[]={"auto","startup","project","file"};
int xideExecutionMode(xide_app* app)
{
    const char* mode=text(get(app->executionTarget,"mode"));
    for(int i=0;mode && i<4;++i)if(!strcmp(mode,modes[i]))return i;
    return XIDE_TARGET_AUTO;
}
const char* xideExecutionProject(xide_app* app) { const char* id=text(get(app->executionTarget,"project"));return id ? id : ""; }
const char* xideExecutionConfiguration(xide_app* app,const xide_project* p)
{
    if(!p)return NULL;
    const char* id=text(get(get(app->executionTarget,"configurations"),p->id));
    if(!id && app->configurationId && *app->configurationId)id=app->configurationId;
    if(!id)id=p->activeConfiguration;
    return id && *id ? id : p->configurationCount ? p->configurations[0].id : NULL;
}
xvalue* xideExecutionAnalysisPacket(xide_app* app,const char* package,int64_t epoch,xide_model_error* error)
{
    xide_workspace view=app->workspaceModel;
    if(view.projectCount>SIZE_MAX/sizeof(*view.projects))return NULL;
    view.projects=view.projectCount ? malloc(view.projectCount*sizeof(*view.projects)) : NULL;
    if(view.projectCount && !view.projects) { if(error) { error->code=XIDE_MODEL_MEMORY;snprintf(error->message,sizeof(error->message),"无法捕获分析配置。"); }return NULL; }
    for(size_t i=0;i<view.projectCount;++i) { view.projects[i]=app->workspaceModel.projects[i];view.projects[i].activeConfiguration=(char*)xideExecutionConfiguration(app,&view.projects[i]); }
    xvalue* result=xideAnalysisConfigurationPacket(&view,app->selectedProject,NULL,package,epoch,error);free(view.projects);return result;
}
int xideExecutionValid(const xvalue* v)
{
    if(!v)return 1;
    const char* mode=text(get(v,"mode"));const xvalue* configs=get(v,"configurations");
    if(!xrtValueIs(v,XVALUE_OBJECT) || !mode || !text(get(v,"project")) || !xrtValueIs(configs,XVALUE_OBJECT) || xrtValueCount(configs)>4096)return 0;
    int found=0;for(int i=0;i<4;++i)if(!strcmp(mode,modes[i]))found=1;
    for(size_t i=0;i<xrtValueCount(configs);++i) { xstrview key={0};const xvalue* item=xrtValueObjectAt(configs,i,&key);const char* id=text(item);if(!key.Size || key.Size>8192 || memchr(key.Data,0,key.Size) || !id || !*id)return 0; }
    return found && (strcmp(mode,"project") || *text(get(v,"project")));
}
xvalue* xideExecutionCapture(xide_app* app)
{
    if(app->executionTarget)return xrtValueDeepClone(app->executionTarget);
    return xrtJsonParse(sv("{\"mode\":\"auto\",\"project\":\"\",\"configurations\":{}}"));
}
int xideExecutionRestore(xide_app* app,const xvalue* v)
{
    if(!xideExecutionValid(v))return 0;
    xvalue* copy=v ? xrtValueDeepClone(v) : NULL;if(v && !copy)return 0;
    xrtValueRelease(app->executionTarget);app->executionTarget=copy;++app->executionRevision;return 1;
}
void xideExecutionReset(xide_app* app) { xrtValueRelease(app->executionTarget);app->executionTarget=NULL;++app->executionRevision; }
int xideExecutionSelect(xide_app* app,int mode,const char* project)
{
    if(mode<0 || mode>3 || !project || strlen(project)>8192 || (mode==XIDE_TARGET_PROJECT && !xideWorkspaceFindProject(&app->workspaceModel,project)))return 0;
    xvalue* v=xideExecutionCapture(app);if(!v)return 0;
    int ok=xrtValueObjectSetNew(v,sv("mode"),xrtValueString(sv(modes[mode]))) && xrtValueObjectSetNew(v,sv("project"),xrtValueString(sv(project)));
    if(ok) { xrtValueRelease(app->executionTarget);app->executionTarget=v;++app->executionRevision; } else xrtValueRelease(v);
    return ok;
}
int xideExecutionSelectConfiguration(xide_app* app,const char* project,const char* configuration)
{
    xide_project* p=xideWorkspaceFindProject(&app->workspaceModel,project);
    if(!p || !xideProjectFindConfiguration(p,configuration))return 0;
    xvalue* v=xideExecutionCapture(app);if(!v)return 0;
    xvalue* configs=(xvalue*)get(v,"configurations");
    if(xrtValueCount(configs)>=4096 && !get(configs,project)) { xrtValueRelease(v);return 0; }
    int ok=xrtValueObjectSetNew(configs,sv(project),xrtValueString(sv(configuration)));
    if(ok) { xrtValueRelease(app->executionTarget);app->executionTarget=v;++app->executionRevision; } else xrtValueRelease(v);
    if(ok && app->workspaceView) { int refreshed=xideWorkspaceViewRefresh(app);if(refreshed!=XUI_OK)app->frameError=refreshed; }
    return ok;
}
void xideExecutionResolve(xide_app* app,int forceFile,xide_execution_context* c)
{
    memset(c,0,sizeof(*c));int mode=xideExecutionMode(app);
    c->file=forceFile || mode==XIDE_TARGET_FILE || (mode==XIDE_TARGET_AUTO && !app->workspaceModel.projectCount);
    if(c->file) {
        c->document=app->activeDocument && app->activeDocument->open ? app->activeDocument : NULL;
        snprintf(c->target,sizeof(c->target),"当前文件：%s",c->document ? c->document->path ? xidePathFilename(c->document->path) : "未命名" : "未打开");
        snprintf(c->config,sizeof(c->config),"单文件默认");c->configurationId="single-file";
        if(!xideTaskViewCanBuildFile(app))c->reason="请打开 C/xlang 源文件；未命名文件会先另存为";
        return;
    }
    c->projectId=mode==XIDE_TARGET_PROJECT ? xideExecutionProject(app) : app->workspaceModel.startupProject;
    c->project=xideWorkspaceFindProject(&app->workspaceModel,c->projectId);
    snprintf(c->target,sizeof(c->target),"%s：%s",mode==XIDE_TARGET_PROJECT ? "项目" : "启动项目",c->project ? c->project->name : c->projectId && *c->projectId ? c->projectId : "未设置");
    if(!c->project || !c->project->available) { c->reason="目标项目不存在或不可用，请重新选择目标";snprintf(c->config,sizeof(c->config),"配置不可用");return; }
    c->configurationId=text(get(get(app->executionTarget,"configurations"),c->projectId));
    if(!c->configurationId && app->configurationId && *app->configurationId) { c->configurationId=app->configurationId;c->commandLineConfiguration=1; }
    c->configurationId=xideExecutionConfiguration(app,c->project);
    c->configuration=xideProjectFindConfiguration(c->project,c->configurationId);
    snprintf(c->config,sizeof(c->config),"%s%s",c->configuration ? c->configuration->name : "配置不存在",c->commandLineConfiguration ? "（命令行）" : "");
    if(!c->configuration)c->reason="目标配置已失效，请重新选择或编辑配置";
}
xide_action_state xideExecutionQuery(xide_app* app,int action)
{
    xide_action_state state={0,0,NULL};xide_execution_context c;xideExecutionResolve(app,action==XIDE_ACTION_BUILD_FILE || action==XIDE_ACTION_RUN_FILE,&c);
    if(c.reason)state.reason=c.reason;
    else if(!app->tasks)state.reason="任务服务尚未就绪";
    else if(action==XIDE_ACTION_TEST) {
        if(c.file)state.reason="单文件目标没有项目测试配置";
        else if(!c.configuration->test.program || !*c.configuration->test.program || !xidePathHasExtension(c.configuration->test.program,".exe"))state.reason="当前配置没有测试程序，请编辑构建、运行与测试配置";
    } else if(!xideTaskViewCompiler(app) || !*xideTaskViewCompiler(app))state.reason="尚未配置编译器，请配置工具链";
    else if((action==XIDE_ACTION_RUN || action==XIDE_ACTION_RUN_FILE) && !c.file && strcmp(c.project->kind,"exe"))state.reason="当前目标是库，不能直接运行";
    state.enabled=state.reason==NULL;return state;
}
