#include "services/resource_inputs.h"
#include "services/tasks.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"resource_inputs_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,info.error); failed=1; goto cleanup; } } while(0)
static int scan(const char* text,const char* path,xide_strings* paths,xide_model_error* e)
{ return xideResourceInputPaths((const unsigned char*)text,strlen(text),path,paths,e); }
static int writeText(const char* p,const char* s)
{ return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static wchar_t* wide(const char* text)
{
 int n=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0); wchar_t* s=n ? malloc((size_t)n*sizeof(*s)) : NULL;
 if(s) MultiByteToWideChar(CP_UTF8,0,text,-1,s,n);
 return s;
}
static int done(xide_tasks* ts,xide_task* t,xide_task_info* info)
{
 uint64_t end=GetTickCount64()+30000;
 do { xideTasksPoll(ts); xideTaskInfo(t,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<end); return 0;
}
static int waiting(xide_task* t)
{
 uint64_t end=GetTickCount64()+10000,cursor=0,dropped=0; char log[4096]={0}; size_t used=0; xide_task_info i={0};
 do {
  size_t n=xideTaskReadLog(t,&cursor,log+used,sizeof(log)-used-1,&dropped); used+=n; log[used]=0;
  if(strstr(log,"BUILD_GATE_WAITING")) return 1;
  xideTaskInfo(t,&i); if(i.state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5);
 } while(GetTickCount64()<end); return 0;
}
static int hasInput(xide_task* t,const char* path)
{
 for(size_t j=0;j<xideTaskInputCount(t);++j) { char* p=xideTaskInputPath(t,j); int match=p && xidePathEqual(p,path); free(p); if(match) return 1; } return 0;
}
static const unsigned char ico[]={0,0,1,0,1,0,1,1,0,0,1,0,32,0,48,0,0,0,22,0,0,0,40,0,0,0,1,0,0,0,2,0,0,0,1,0,32,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0};
static const char* xml="<?xml version=\"1.0\" encoding=\"UTF-8\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity name=\"ResourceInputs\" version=\"1.0.0.0\" type=\"win32\"/></assembly>";
static const char* resource="{\"version\":{\"fileVersion\":\"9.8.7.6\"},\"icon\":{\"path\":\"图标😀.ico\",\"base64\":\"ignored\"},\"manifest\":\"清单.xml\",\"future\":{\"values\":set[char(\"你\"),bytes(\"AQID\")],\"map\":intmap{-1:[float(\"nan\"),time(\"2000-01-02T03:04:05Z\")]}}}";
int main(void)
{
 int failed=0,checks=0,argc=0; wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&argc); char** argv=NULL;
 xide_model_error error={0}; xide_task_info info={0}; xide_strings paths={0}; xide_project project={0},budget={0}; xide_tasks* tasks=NULL;
 char *root=NULL,*manifest=NULL,*entry=NULL,*config=NULL,*icon=NULL,*xmlPath=NULL,*bin=NULL,*fixture=NULL,*output=NULL,*alias=NULL;
 wchar_t *iconW=NULL,*aliasW=NULL; HANDLE gate=NULL; unsigned char* old=NULL; size_t oldSize=0;
 CHECK(args && (argc==2 || argc==3)); argv=calloc((size_t)argc,sizeof(*argv)); CHECK(argv);
 for(int i=0;i<argc;++i) { int n=WideCharToMultiByte(CP_UTF8,0,args[i],-1,NULL,0,NULL,NULL); argv[i]=n ? malloc((size_t)n) : NULL; CHECK(argv[i]); WideCharToMultiByte(CP_UTF8,0,args[i],-1,argv[i],n,NULL,NULL); }
 root=xidePathResolve(argv[1],"中文 资源输入"); CHECK(root && xrtDirCreateAll(root));
 manifest=xidePathResolve(root,"项目.xlp"); entry=xidePathResolve(root,"main.xl"); config=xidePathResolve(root,"app.xlon"); icon=xidePathResolve(root,"图标😀.ico"); xmlPath=xidePathResolve(root,"清单.xml"); alias=xidePathResolve(root,"图标 alias.ico");
 bin=xidePathDirectory(argv[0]); fixture=bin ? xidePathResolve(bin,"task_test.exe") : NULL;
 CHECK(manifest && entry && config && icon && xmlPath && alias && fixture);
 CHECK(scan(resource,config,&paths,&error) && paths.count==2 && xidePathEqual(paths.items[0],icon) && xidePathEqual(paths.items[1],xmlPath));
 CHECK(scan("{\"icon\":\"图标\\ud83d\\ude00.ico\",\"manifest\":{\"path\":\"图标😀.ico\"}}",config,&paths,&error) && paths.count==1 && xidePathEqual(paths.items[0],icon));
 CHECK(scan("\xef\xbb\xbf{\"icon\":{\"path\":null,\"base64\":\"AA==\"},\"manifest\":{\"text\":\"<path>ignored</path>\"}}",config,&paths,&error) && !paths.count);
 CHECK(scan("{/* } */\"i\\u0063on\": \"图标😀.ico\" /* [, ] */,\"future\":newType({\"x\":[1,\"\\\"})\"]}),\"manifest\":null}",config,&paths,&error) && paths.count==1);
 CHECK(scan("{\"future\\u0000key\":intmap{1:bytes(\"AA==\")},\"icon\":\"图标😀.ico\"}",config,&paths,&error) && paths.count==1);
 char* preserved=paths.items[0];
 const char* invalid[]={"[]","{} trailing","{\"icon\":\"\"}","{\"icon\":\"a\\u0000b\"}","{\"icon\":\"a\",\"icon\":\"b\"}","{\"icon\":{\"path\":\"a\",\"path\":\"b\"}}","{\"x\":[)}","{\"x\":tag(\"unfinished\"}","{\"icon\":\"bad\\q\"}","{\"x\":/*unterminated}"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) CHECK(!scan(invalid[i],config,&paths,&error) && paths.count==1 && paths.items[0]==preserved);
 unsigned char nul[]={'{','}',0}; CHECK(!xideResourceInputPaths(nul,sizeof(nul),config,&paths,&error) && paths.items[0]==preserved);
 CHECK(!xideResourceInputPaths((const unsigned char*)"{}",16u*1024u*1024u+1,config,&paths,&error));
 xideResourceInputPathsClear(&paths);
 CHECK(writeText(entry,"FAKE_GATE\n") && writeText(config,resource) && xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}) && writeText(xmlPath,xml));
 CHECK(xideProjectCreate(&project,manifest,"素材输入",entry,&error) && xideProjectAddFile(&project,config,"resource-config",&error) && xideProjectSave(&project,NULL,&error));
 output=xidePathResolve(NULL,project.configurations[0].output); CHECK(output);
 tasks=xideTasksCreate(); CHECK(tasks);
 wchar_t gateName[160]; swprintf(gateName,160,L"Local\\xide-resource-input-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 gate=CreateEventW(NULL,TRUE,FALSE,gateName); CHECK(gate && SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",gateName));
 xide_task* task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && waiting(task) && hasInput(task,icon) && hasInput(task,xmlPath));
 CHECK(SetEvent(gate) && done(tasks,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.ran && xideTaskInputCount(task)==5);
 old=xrtFileReadAll(output,&oldSize); CHECK(old && oldSize);
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,1,&error); CHECK(task && waiting(task));
 CHECK(writeText(xmlPath,"changed external manifest bytes") && SetEvent(gate) && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && strstr(info.error,"changed while"));
 size_t nowSize=0; unsigned char* now=xrtFileReadAll(output,&nowSize); int same=now && oldSize==nowSize && !memcmp(old,now,nowSize); xrtFree(now); CHECK(same && writeText(xmlPath,xml));
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,1,&error); CHECK(task && waiting(task));
 xideTasksInvalidateInput(tasks,icon); CHECK(done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
 iconW=wide(icon); aliasW=wide(alias); CHECK(iconW && aliasW && CreateHardLinkW(aliasW,iconW,NULL));
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,1,&error); CHECK(task && waiting(task));
 xideTasksInvalidateInput(tasks,alias); CHECK(done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
 char* dirtyPaths[]={xmlPath}; xide_strings dirty={.items=dirtyPaths,.count=1};
 task=xideTasksBuildGuarded(tasks,&project,NULL,fixture,1,&dirty,&error);
 CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran);
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && waiting(task));
 xide_task* guarded=xideTasksBuildGuarded(tasks,&project,NULL,fixture,0,&dirty,&error); CHECK(guarded && guarded!=task);
 CHECK(done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && done(tasks,guarded,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built);
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && waiting(task));
 CHECK(xideProjectSetMetadata(&project,"素材输入新快照",project.description,project.kind,&error) && xideProjectSave(&project,NULL,&error));
 xide_task* updated=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(updated && updated!=task);
 CHECK(SetEvent(gate) && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && done(tasks,updated,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built);
 CHECK(ResetEvent(gate)); task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && waiting(task));
 xide_task* queued=xideTasksBuild(tasks,&project,NULL,fixture,1,&error); CHECK(queued && queued!=task); xideTasksInvalidateInput(tasks,alias);
 CHECK(done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && done(tasks,queued,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
 CHECK(writeText(config,"{\"icon\":\"missing.ico\"}")); task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && strstr(info.error,"read build input"));
 unsigned char* large=calloc(16u*1024u*1024u+1,1); CHECK(large);
 int largeWritten=xrtFileWriteAtomic(icon,(xbytesview){large,16u*1024u*1024u+1}); free(large); CHECK(largeWritten && writeText(config,resource));
 task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && strstr(info.error,"read build input"));
 CHECK(xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}));
 /* Use the exact configured output; resource strings must be JSON-escaped. */
 xvalue* r=xrtValueObject(); CHECK(r && xrtValueObjectSetNew(r,(xstrview){"icon",4},xrtValueString((xstrview){output,strlen(output)}))); size_t n=0; char* body=xrtJsonStringify(r,false,&n); xrtValueRelease(r); CHECK(body); int wrote=writeText(config,body); xrtFree(body); CHECK(wrote);
 task=xideTasksBuild(tasks,&project,NULL,fixture,0,&error); CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && strstr(info.error,"overwrite a resource asset"));
 /* Eight distinct hard-link paths read 128 MiB plus the captured project/config
    bytes. The service must count the original batch when adding asset inputs. */
 char* budgetManifest=xidePathResolve(root,"budget.xlp"); CHECK(budgetManifest);
 int budgetOkay=xideProjectCreate(&budget,budgetManifest,"预算测试",entry,&error); free(budgetManifest); CHECK(budgetOkay);
 large=calloc(16u*1024u*1024u,1); CHECK(large); largeWritten=xrtFileWriteAtomic(icon,(xbytesview){large,16u*1024u*1024u}); free(large); CHECK(largeWritten);
 for(int i=0;i<8;++i) {
  char name[64],json[128]; snprintf(name,sizeof(name),"budget-%d.ico",i); char* p=xidePathResolve(root,name); wchar_t* w=p ? wide(p) : NULL;
  int linked=w && CreateHardLinkW(w,iconW,NULL); free(p); free(w); CHECK(linked);
  snprintf(json,sizeof(json),"{\"icon\":\"budget-%d.ico\"}",i); snprintf(name,sizeof(name),"budget-%d.json",i); p=xidePathResolve(root,name);
  budgetOkay=p && writeText(p,json) && xideProjectAddFile(&budget,p,"resource-config",&error); free(p); CHECK(budgetOkay);
 }
 CHECK(xideProjectSave(&budget,NULL,&error)); task=xideTasksBuild(tasks,&budget,NULL,fixture,0,&error); xideProjectClear(&budget);
 CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && strstr(info.error,"128 MiB budget"));
 CHECK(xrtFileWriteAtomic(icon,(xbytesview){ico,sizeof(ico)}));
 if(argc==3) {
  CHECK(writeText(entry,"func main(...) as int\n print(\"资源输入真实混编\")\n return 0\nend\n") && writeText(config,resource));
  char* native=xidePathResolve(root,"native.c"); CHECK(native); int okay=writeText(native,"int resource_input_native(void) { return 42; }\n") && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error); free(native); CHECK(okay);
  task=xideTasksBuild(tasks,&project,NULL,argv[2],1,&error); CHECK(task && done(tasks,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && hasInput(task,icon) && hasInput(task,xmlPath));
  free(iconW); iconW=wide(output); CHECK(iconW);
  HMODULE module=LoadLibraryExW(iconW,NULL,LOAD_LIBRARY_AS_DATAFILE); CHECK(module);
  HRSRC res=FindResourceW(module,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(24)); HGLOBAL data=res ? LoadResource(module,res) : NULL; void* raw=data ? LockResource(data) : NULL;
  same=raw && SizeofResource(module,res)==strlen(xml) && !memcmp(raw,xml,strlen(xml)); FreeLibrary(module); CHECK(same);
  puts("resource_inputs_test real compiler passed: typed XLON C/xlang build/run and original PE manifest bytes.");
 }
 printf("resource_inputs_test passed: %d checks; opaque XLON projection, BOM/Unicode/priority/dedup/atomic errors, actual gated build, disk and queued memory invalidation, hard-link alias, missing input and output protection.\n",checks);
cleanup:
 if(gate) SetEvent(gate);
 xideTasksDestroy(tasks); if(gate) CloseHandle(gate); SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",NULL);
 xideProjectClear(&project); xideProjectClear(&budget); xideResourceInputPathsClear(&paths); xrtFree(old);
 char* all[]={root,manifest,entry,config,icon,xmlPath,bin,fixture,output,alias}; for(size_t i=0;i<sizeof(all)/sizeof(*all);++i) free(all[i]); free(iconW); free(aliasW);
 if(argv) { for(int i=0;i<argc;++i) free(argv[i]); free(argv); } if(args) LocalFree(args); return failed;
}
