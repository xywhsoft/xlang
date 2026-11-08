#define _WIN32_WINNT 0x0601
#include "services/template_generation.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include "platform/process.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

typedef struct template_output {char* path;unsigned char* data;size_t size;char* kind;BY_HANDLE_FILE_INFORMATION identity;int written;} template_output;
struct xide_template_job {
    HANDLE thread;volatile LONG done,cancel;
    xide_template_definition definition;xvalue *values,*fieldErrors;
    char *name,*location,*workspaceName,*compiler,*scratch,*project,*workspace,*text;
    int projectDirectory,workspaceMode,workspaceDirectory,success;
    char error[512];
};
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static xvalue* get(const xvalue* v,const char* k){return v && xrtValueIs(v,XVALUE_OBJECT) ? xrtValueObjectGet(v,sv(k)):NULL;}
static int put(xvalue* v,const char* k,const char* s){return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s)));}
static char* copy(const char* s){size_t n=strlen(s)+1;char* p=malloc(n);if(p)memcpy(p,s,n);return p;}
static int failure(xide_template_job* j,const char* format,...){va_list args;va_start(args,format);vsnprintf(j->error,sizeof(j->error),format,args);va_end(args);return 0;}
static int cancelled(xide_template_job* j){if(!InterlockedCompareExchange(&j->cancel,0,0))return 0;failure(j,"创建已取消；已生成的工程不会被再次生成。");return 1;}
int xideTemplateNameValid(const char* name){return xideProjectTemplateNameValid(name,NULL);}
static int safeRelative(const char* path){
    if(!path||!*path||strlen(path)>1024||path[0]=='/'||strchr(path,'\\')||strchr(path,':'))return 0;
    const char* p=path;while(*p){const char* slash=strchr(p,'/');size_t n=slash ? (size_t)(slash-p):strlen(p);char part[256];if(!n||n>=sizeof(part))return 0;memcpy(part,p,n);part[n]=0;if(!xideTemplateNameValid(part))return 0;p=slash ? slash+1:p+n;if(slash&&!*p)return 0;}return 1;
}
static char* literal(const char* text){xvalue* v=xrtValueString(sv(text));size_t n=0;char* x=v ? xrtJsonStringify(v,0,&n):NULL;char* out=x ? copy(x):NULL;xrtFree(x);xrtValueRelease(v);return out;}
static int writeText(const char* path,const char* text){return xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static char* render(const char* input,const xvalue* values){
    size_t capacity=strlen(input)+1,used=0;char* out=malloc(capacity);if(!out)return NULL;
    for(const char* p=input;*p;){const char* end=NULL;char* owned=NULL;const char* text=p;size_t n=1;
        if(p[0]=='$'&&p[1]=='{'&&(end=strchr(p+2,'}'))){size_t keySize=(size_t)(end-p-2);if(keySize>=256){free(out);return NULL;}char key[256];memcpy(key,p+2,keySize);key[keySize]=0;xvalue* value=get(values,key);if(!value){free(out);return NULL;}xstrview str={0};if(xrtValueIs(value,XVALUE_STRING)&&xrtValueGetString(value,&str)){text=str.Data;n=str.Size;}else{size_t len=0;char* json=xrtJsonStringify(value,0,&len);owned=json ? copy(json):NULL;xrtFree(json);if(!owned){free(out);return NULL;}text=owned;n=strlen(owned);}}
        if(n>16*1024*1024-used){free(owned);free(out);return NULL;}if(used+n+1>capacity){size_t next=(used+n+1)*2;char* q=realloc(out,next);if(!q){free(owned);free(out);return NULL;}out=q;capacity=next;}memcpy(out+used,text,n);used+=n;free(owned);p=end ? end+1:p+1;
    }out[used]=0;return out;
}
static xvalue* callback(xide_template_job* j,xvalue* context){
    char* token=xideCreateId("template");char* dir=token ? xidePathResolve(j->scratch,token):NULL;free(token);
    if(!dir||!xideIoCreateDirectories(dir)){free(dir);failure(j,"无法创建模板任务目录。");return NULL;}
    char* request=xidePathResolve(dir,"request.json"),*result=xidePathResolve(dir,"result.json"),*entry=xidePathResolve(dir,"entry.xl"),*log=xidePathResolve(dir,"generator.log"),*module=xidePathResolve(j->definition.root,j->definition.module);
    char *qRequest=request ? literal(request):NULL,*qResult=result ? literal(result):NULL,*qModule=module ? literal(module):NULL;size_t n=0;char* encoded=xrtJsonStringify(context,1,&n);char* source=NULL;
    int okay=request&&result&&entry&&log&&qRequest&&qResult&&qModule&&encoded;
    if(okay){size_t cap=strlen(qRequest)+strlen(qResult)*2+strlen(qModule)+strlen(j->definition.validate)+strlen(j->definition.create)+1024;source=malloc(cap);okay=source!=NULL;
        if(okay)snprintf(source,cap,"import %s as __xide_template\nvar __xide_context = json.parse(file.readText(%s))\nvar __xide_result = json.parse(\"{}\")\n%s%s%s\n    __xide_result = __xide_template.%s(__xide_context)\n%sfile.writeText(%s, json.stringify(__xide_result))\n",qModule,qRequest,*j->definition.validate ? "var __xide_errors = __xide_template.":"",*j->definition.validate ? j->definition.validate:"",*j->definition.validate ? "(__xide_context)\nif __xide_errors == null then":"",j->definition.create,*j->definition.validate ? "else\n    __xide_result[\"errors\"] = __xide_errors\nend\n":"",qResult);
        okay=okay&&writeText(request,encoded)&&writeText(entry,source);
    }
    xide_process* process=NULL;if(okay){const char* args[]={"run",entry};xide_process_spec spec={0};spec.program=j->compiler;spec.directory=j->definition.root;spec.arguments=args;spec.argumentCount=2;spec.logPath=log;spec.separateStderr=1;process=xideProcessStart(&spec,j->error,sizeof(j->error));okay=process!=NULL;}
    ULONGLONG start=GetTickCount64();xide_process_info info={0};char tail[384]={0};size_t used=0;
    while(okay){char bytes[512];size_t got=xideProcessReadError(process,bytes,sizeof(bytes));if(got){size_t keep=got<sizeof(tail)-1 ? got:sizeof(tail)-1;memcpy(tail,bytes+got-keep,keep);tail[keep]=0;used=keep;}while(xideProcessRead(process,bytes,sizeof(bytes))){}
        xideProcessGetInfo(process,&info);if(info.state==XIDE_PROCESS_FINISHED)break;
        if(InterlockedCompareExchange(&j->cancel,0,0)||GetTickCount64()-start>120000){xideProcessCancel(process);okay=0;failure(j,"模板生成取消或超时。日志：%s",log);break;}Sleep(10);
    }
    if(process){xideProcessDestroy(process);}xvalue* answer=NULL;
    if(okay&&info.exit.Code==0&&!info.ioError){size_t size=0;unsigned char* data=xideIoReadAllLimit(result,16*1024*1024,&size);if(data){answer=xrtJsonParse((xstrview){(char*)data,size});xrtFree(data);}}
    if(!answer&&!j->error[0])failure(j,"模板回调失败：%s。任务资料：%s",used ? tail:info.error,dir);
    free(source);xrtFree(encoded);free(qRequest);free(qResult);free(qModule);free(request);free(result);free(entry);free(log);free(module);free(dir);return answer;
}
static void outputsClear(template_output* files,size_t count){for(size_t i=0;i<count;++i){free(files[i].path);free(files[i].data);free(files[i].kind);}free(files);}
static int addOutput(template_output** files,size_t* count,const char* path,const unsigned char* bytes,size_t size,const char* kind){
    if(!safeRelative(path)||*count>=1024||size>16*1024*1024)return 0;
    for(size_t i=0;i<*count;++i)if(!_stricmp((*files)[i].path,path))return 0;
    template_output f={0};f.path=copy(path);f.kind=copy(kind);f.data=malloc(size+1);if(!f.path||!f.kind||!f.data){free(f.path);free(f.kind);free(f.data);return 0;}memcpy(f.data,bytes,size);f.data[size]=0;f.size=size;
    void* next=realloc(*files,(*count+1)*sizeof(**files));if(!next){free(f.path);free(f.kind);free(f.data);return 0;}*files=next;(*files)[(*count)++]=f;return 1;
}
static int collect(xide_template_job* j,xvalue* plan,template_output** files,size_t* count){
    const xide_project_template* builtin=j->definition.builtin&&j->definition.project ? xideProjectTemplateFind(j->definition.id):NULL;
    if(builtin){for(size_t i=0;i<builtin->fileCount;++i)if(!addOutput(files,count,builtin->files[i].path,(const unsigned char*)builtin->files[i].content,strlen(builtin->files[i].content),builtin->files[i].kind))return failure(j,"无法准备内置文件：%s",builtin->files[i].path);return 1;}
    xvalue* list=plan ? get(plan,"files"):j->definition.files;if(!list||!xrtValueIs(list,XVALUE_ARRAY)||!xrtValueCount(list))return failure(j,"生成计划必须包含 files 数组。");
    size_t total=0;
    for(size_t i=0;i<xrtValueCount(list);++i){xvalue* f=xrtValueArrayGet(list,i);const char* target=xideTemplateString(f,"path",""),*source=xideTemplateString(f,"source",""),*kind=xideTemplateString(f,"kind","text");
        bool expand=plan==NULL;if(xrtValueIs(get(f,"expand"),XVALUE_BOOL))xrtValueGetBool(get(f,"expand"),&expand);char* output=expand ? render(target,j->values):copy(target);char* text=NULL;unsigned char* data=NULL;size_t size=0;bool binary=false;if(xrtValueIs(get(f,"binary"),XVALUE_BOOL))xrtValueGetBool(get(f,"binary"),&binary);
        if(*source){if(!safeRelative(source)){free(output);return failure(j,"模板资源路径无效。");}char* path=xidePathResolve(j->definition.root,source);data=path ? xideIoReadAllLimit(path,16*1024*1024,&size):NULL;free(path);}
        else {xstrview content={0};if(xrtValueGetString(get(f,"content"),&content)&&!memchr(content.Data,0,content.Size)){size=content.Size;data=malloc(size+1);if(data)memcpy(data,content.Data,size+1);}}
        int okay=data!=NULL;
        if(okay&&!binary){if(memchr(data,0,size)||(size&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(const char*)data,(int)size,NULL,0)))okay=0;else{char* terminated=malloc(size+1);if(terminated){memcpy(terminated,data,size);terminated[size]=0;text=expand ? render(terminated,j->values):copy(terminated);free(terminated);}okay=text!=NULL;}}
        const unsigned char* bytes=text ? (const unsigned char*)text:data;size_t length=text ? strlen(text):size;okay=okay&&output&&length<=64*1024*1024-total&&addOutput(files,count,output,bytes,length,kind);total+=length;free(output);free(text);if(*source)xrtFree(data);else free(data);
        if(!okay)return failure(j,"生成文件路径、内容、重复项或资源大小无效。");
    }return 1;
}
static int writeNew(const char* path,template_output* f){
    wchar_t* wide=xidePathNativeWide(path);HANDLE h=wide ? CreateFileW(wide,GENERIC_WRITE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;free(wide);if(h==INVALID_HANDLE_VALUE)return 0;
    int okay=GetFileInformationByHandle(h,&f->identity)!=0;f->written=okay;size_t at=0;while(okay&&at<f->size){DWORD done=0,want=(DWORD)((f->size-at)>65536 ? 65536:f->size-at);if(!WriteFile(h,f->data+at,want,&done,NULL)||!done){okay=0;break;}at+=done;}
    okay=okay&&FlushFileBuffers(h);CloseHandle(h);return okay;
}
/* Clean only known files with matching physical identity. Never recursively
 * delete an unrecognised directory or follow a replaced path. */
static void rollbackFile(const char* path,const template_output* f){
    if(!f->written){return;}wchar_t* wide=xidePathNativeWide(path);HANDLE h=wide ? CreateFileW(wide,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL):INVALID_HANDLE_VALUE;free(wide);if(h==INVALID_HANDLE_VALUE)return;
    BY_HANDLE_FILE_INFORMATION now={0};FILE_DISPOSITION_INFO remove={TRUE};if(GetFileInformationByHandle(h,&now)&&now.dwVolumeSerialNumber==f->identity.dwVolumeSerialNumber&&now.nFileIndexHigh==f->identity.nFileIndexHigh&&now.nFileIndexLow==f->identity.nFileIndexLow&&now.nNumberOfLinks==1&&!(now.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))SetFileInformationByHandle(h,FileDispositionInfo,&remove,sizeof(remove));CloseHandle(h);
}
static int publish(xide_template_job* j,const char* directory,template_output* files,size_t count){
    char* parent=xidePathDirectory(directory),*id=xideCreateId(".xide-template"),*stage=parent&&id ? xidePathResolve(parent,id):NULL;free(id);int nested=j->projectDirectory;
    if(!parent||!stage||!xidePathExists(parent)){free(parent);free(stage);return failure(j,"工程父目录不存在。");}
    if(nested&&xidePathExists(directory)){free(parent);free(stage);return failure(j,"工程目录已存在，请更换名称。");}
    wchar_t* w=xidePathNativeWide(stage);int okay=w&&CreateDirectoryW(w,NULL);HANDLE stageHandle=okay ? CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL):INVALID_HANDLE_VALUE;okay=okay&&stageHandle!=INVALID_HANDLE_VALUE;free(w);size_t written=0;
    for(size_t i=0;okay&&i<count;++i){if(cancelled(j)){okay=0;break;}char* path=xidePathResolve(stage,files[i].path),*dir=path ? xidePathDirectory(path):NULL;okay=path&&dir&&xideIoCreateDirectories(dir)&&writeNew(path,&files[i]);if(files[i].written)written=i+1;free(path);free(dir);}
    if(okay&&cancelled(j))okay=0;
    if(okay&&nested){wchar_t* target=xidePathNativeWide(directory);size_t chars=target ? wcslen(target):0;size_t size=offsetof(FILE_RENAME_INFO,FileName)+(chars+1)*sizeof(wchar_t);FILE_RENAME_INFO* info=target ? calloc(1,size):NULL;
        okay=info!=NULL;if(info){info->FileNameLength=(DWORD)(chars*sizeof(wchar_t));memcpy(info->FileName,target,(chars+1)*sizeof(wchar_t));okay=SetFileInformationByHandle(stageHandle,FileRenameInfo,info,(DWORD)size)!=0;}free(info);free(target);}
    if(okay&&!nested){for(size_t i=0;i<count;++i){char* path=xidePathResolve(directory,files[i].path);int exists=!path||xidePathExists(path);free(path);if(exists){okay=failure(j,"目标文件已存在；不会覆盖现有内容。");break;}}
        size_t moved=0;for(;okay&&moved<count;++moved){char* from=xidePathResolve(stage,files[moved].path),*to=xidePathResolve(directory,files[moved].path),*dir=to ? xidePathDirectory(to):NULL;okay=from&&to&&dir&&xideIoCreateDirectories(dir)&&xideIoMove(from,to,0);free(from);free(to);free(dir);}
        if(!okay)for(size_t i=0;i<moved;++i){char* path=xidePathResolve(directory,files[i].path);if(path)rollbackFile(path,&files[i]);free(path);}}
    if(!okay){for(size_t i=0;i<written;++i){char* path=xidePathResolve(stage,files[i].path);if(path)rollbackFile(path,&files[i]);free(path);}if(!j->error[0])failure(j,"写入或发布工程失败；已有文件未覆盖。暂存位置：%s",stage);}
    /* Remove only empty, known parent paths below this newly owned stage. */
    for(size_t i=0;i<count;++i){char* path=xidePathResolve(stage,files[i].path),*dir=path ? xidePathDirectory(path):NULL;free(path);size_t rootSize=strlen(stage);
        while(dir&&strlen(dir)>rootSize&&!_strnicmp(dir,stage,rootSize)&&dir[rootSize]=='/'){wchar_t* native=xidePathNativeWide(dir);DWORD flags=native ? GetFileAttributesW(native):INVALID_FILE_ATTRIBUTES;if(native&&flags!=INVALID_FILE_ATTRIBUTES&&!(flags&FILE_ATTRIBUTE_REPARSE_POINT))RemoveDirectoryW(native);free(native);char* up=xidePathDirectory(dir);free(dir);dir=up;}free(dir);}
    /* Empty directory cleanup only; nonempty staging evidence is retained. */
    if(stageHandle!=INVALID_HANDLE_VALUE){CloseHandle(stageHandle);}w=xidePathNativeWide(stage);if(w){RemoveDirectoryW(w);free(w);}if(!okay&&xidePathExists(stage)){size_t used=strlen(j->error);snprintf(j->error+used,sizeof(j->error)-used,"\n暂存资料：%s",stage);}free(parent);free(stage);return okay;
}
static DWORD WINAPI generate(void* user){
    xide_template_job* j=user;template_output* files=NULL;size_t count=0;xvalue *context=NULL,*plan=NULL;xide_project project={0};xide_workspace workspace={0};char *base=NULL,*directory=NULL,*entry=NULL;int okay=1;
    if(j->definition.project&&(!xideTemplateNameValid(j->name)||(j->workspaceMode==1&&!xideTemplateNameValid(j->workspaceName)))){failure(j,"工程／工作空间名称无效。");goto done;}
    if(j->definition.project&&!xidePathIsDirectory(j->location)){failure(j,"存放位置必须是已有、可访问的目录。");goto done;}
    base=xidePathResolve(NULL,j->location);
    if(j->workspaceMode==1&&j->workspaceDirectory){char* next=base ? xidePathResolve(base,j->workspaceName):NULL;free(base);base=next;}
    if(j->definition.project&&j->workspaceMode==1&&j->workspaceDirectory&&base&&xidePathExists(base)){failure(j,"工作空间目录已存在，请更换名称或取消创建子目录。");goto done;}
    directory=base ? (j->projectDirectory ? xidePathResolve(base,j->name):copy(base)):NULL;
    if(!directory){failure(j,"目标位置无效。");goto done;}
    put(j->values,"project.name",j->name);put(j->values,"project.directory",directory);put(j->values,"workspace.name",j->workspaceName);
    context=xrtValueObject();xvalue* projectContext=xrtValueObject(),*workspaceContext=xrtValueObject();
    okay=context&&projectContext&&workspaceContext&&put(projectContext,"name",j->name)&&put(projectContext,"directory",directory)&&put(workspaceContext,"name",j->workspaceName)&&put(workspaceContext,"mode",j->workspaceMode==1 ? "create":j->workspaceMode==2 ? "current":"none")&&xrtValueObjectSet(context,sv("project"),projectContext)&&xrtValueObjectSet(context,sv("workspace"),workspaceContext)&&xrtValueObjectSet(context,sv("fields"),j->values)&&put(context,"templateId",j->definition.id)&&put(context,"platform","win_x64");xrtValueRelease(projectContext);xrtValueRelease(workspaceContext);
    if(!okay){failure(j,"无法准备模板上下文。");goto done;}
    if(*j->definition.module){plan=callback(j,context);if(!plan)goto done;if(get(plan,"errors")){j->fieldErrors=xrtValueDeepClone(get(plan,"errors"));size_t size=0;char* text=xrtJsonStringify(get(plan,"errors"),0,&size);failure(j,"模板校验失败：%s",text ? text:"");xrtFree(text);goto done;}}
    if(!collect(j,plan,&files,&count))goto done;
    if(!j->definition.project){if(count!=1||memchr(files[0].data,0,files[0].size)){failure(j,"独立文件模板必须生成一个文本文件。");goto done;}j->text=copy((char*)files[0].data);j->success=j->text!=NULL;goto done;}
    entry=copy(plan ? xideTemplateString(plan,"entry",files[0].path):files[0].path);if(!entry||!safeRelative(entry)){failure(j,"模板入口路径无效。");goto done;}int entryFound=0;for(size_t i=0;i<count;++i)if(!strcmp(entry,files[i].path))entryFound=1;
    if(!entryFound){failure(j,"模板入口未包含在生成文件中。");goto done;}
    char* manifest=xidePathResolve(directory,"project.xlp");xide_model_error error={0};const xide_project_template* builtin=j->definition.builtin ? xideProjectTemplateFind(j->definition.id):NULL;const char* kind=builtin ? builtin->kind:plan ? xideTemplateString(plan,"kind","exe"):"exe";
    okay=manifest&&xideProjectCreate(&project,manifest,j->name,entry,&error)&&xideProjectSetMetadata(&project,j->name,j->definition.description,kind,&error);
    for(size_t i=0;okay&&i<count;++i)if(strcmp(files[i].path,entry))okay=xideProjectAddFile(&project,files[i].path,files[i].kind,&error);
    if(okay&&strcmp(kind,"exe")){xvalue* configurations=xideProjectConfigurationsJson(&project);xvalue* first=configurations ? xrtValueArrayGet(configurations,0):NULL;okay=first&&put(first,"output","build/development/app.dll")&&xideProjectApplyConfigurations(&project,configurations,"development",&error);xrtValueRelease(configurations);}
    xvalue* exported=okay ? xideProjectExport(&project,&error):NULL;size_t length=0;char* json=exported ? xrtJsonStringify(exported,1,&length):NULL;xrtValueRelease(exported);
    okay=okay&&json&&addOutput(&files,&count,"project.xlp",(unsigned char*)json,length,"project");xrtFree(json);
    if(okay&&j->workspaceMode==1&&j->workspaceDirectory&&!xidePathExists(base))okay=xideIoCreateDirectories(base);
    if(okay)okay=!cancelled(j)&&publish(j,directory,files,count);
    if(okay){j->project=manifest;manifest=NULL;}
    free(manifest);if(!okay){if(!j->error[0])failure(j,"%s",error.message[0] ? error.message:"工程生成失败。");goto done;}
    if(j->workspaceMode==1){size_t n=strlen(j->workspaceName)+5;char* name=malloc(n);if(name)snprintf(name,n,"%s.xlw",j->workspaceName);char* path=name ? xidePathResolve(base,name):NULL;free(name);
        okay=path&&!xidePathExists(path)&&xideWorkspaceCreate(&workspace,j->workspaceName,&error)&&xideWorkspaceAddProject(&workspace,j->project,NULL,&error);
        if(okay)okay=xideWorkspaceSetStartup(&workspace,workspace.projects[0].id,&error)&&xideWorkspaceSave(&workspace,path,&error);
        if(okay){j->workspace=path;path=NULL;}free(path);
        if(!okay){failure(j,"工程已生成：%s；工作空间未保存：%s。可直接打开工程，勿重复生成。",j->project,error.message[0] ? error.message:"路径已存在或保存失败");goto done;}
    }
    j->success=1;
done:outputsClear(files,count);xrtValueRelease(context);xrtValueRelease(plan);xideProjectClear(&project);xideWorkspaceClear(&workspace);free(base);free(directory);free(entry);xrtErrorFree(xrtTakeError());InterlockedExchange(&j->done,1);return 0;
}
xide_template_job* xideTemplateJobStart(const xide_template_request* q,xide_model_error* e){
    if(e){memset(e,0,sizeof(*e));}xide_template_job* j=calloc(1,sizeof(*j));if(!j)return NULL;
    int okay=q&&q->definition&&xideTemplateDefinitionCopy(&j->definition,q->definition);xvalue* checked=NULL;
    if(okay)okay=xideTemplateFieldsValidate(q->definition->fields,q->values,&checked,e);
    j->values=checked;j->name=copy(q&&q->name ? q->name:"");j->location=copy(q&&q->location ? q->location:"");j->workspaceName=copy(q&&q->workspaceName ? q->workspaceName:"");j->compiler=copy(q&&q->compiler ? q->compiler:"");j->scratch=copy(q&&q->scratch ? q->scratch:"");
    if(q){j->projectDirectory=q->projectDirectory;j->workspaceDirectory=q->workspaceDirectory;j->workspaceMode=q->workspaceMode;}
    if(okay&&j->name&&j->location&&j->workspaceName&&j->compiler&&j->scratch)j->thread=CreateThread(NULL,0,generate,j,0,NULL);
    if(!j->thread){if(e&&!e->message[0]){e->code=XIDE_MODEL_MEMORY;snprintf(e->message,sizeof(e->message),"无法准备或启动模板任务。");}xideTemplateJobDestroy(j);return NULL;}return j;
}
int xideTemplateJobDone(xide_template_job* j){return j&&InterlockedCompareExchange(&j->done,0,0);}
void xideTemplateJobCancel(xide_template_job* j){if(j)InterlockedExchange(&j->cancel,1);}
const char* xideTemplateJobProject(const xide_template_job* j){return j&&j->project ? j->project:"";}
const char* xideTemplateJobWorkspace(const xide_template_job* j){return j&&j->workspace ? j->workspace:"";}
const char* xideTemplateJobText(const xide_template_job* j){return j&&j->text ? j->text:"";}
const char* xideTemplateJobError(const xide_template_job* j){return j ? j->error:"";}
const xvalue* xideTemplateJobFieldErrors(const xide_template_job* j){return j ? j->fieldErrors:NULL;}
int xideTemplateJobSucceeded(const xide_template_job* j){return j&&j->success;}
void xideTemplateJobDestroy(xide_template_job* j){if(!j)return;if(j->thread){xideTemplateJobCancel(j);WaitForSingleObject(j->thread,INFINITE);CloseHandle(j->thread);}xideTemplateDefinitionClear(&j->definition);xrtValueRelease(j->values);xrtValueRelease(j->fieldErrors);free(j->name);free(j->location);free(j->workspaceName);free(j->compiler);free(j->scratch);free(j->project);free(j->workspace);free(j->text);free(j);}
