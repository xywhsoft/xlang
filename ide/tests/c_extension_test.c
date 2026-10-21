#define _WIN32_WINNT 0x0601
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/services/c_completion_job.c"
static int checks,failed;
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,cjsv(k)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0};return xrtValueGetString(v,&s) ? s.Data : ""; }
static int put(xvalue* v,const char* k,xvalue* x) { return x && xrtValueObjectSetNew(v,cjsv(k),x); }
static int save(const char* root,const char* name,const xvalue* v) { char* p=xidePathResolve(root,name);size_t n=0;char* s=xrtJsonStringify(v,true,&n);int okay=p && s && xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)s,n});free(p);xrtFree(s);return okay; }
static int state(xide_c_completion_job* j,enum xide_c_job_state wanted,char* message) { uint64_t until=GetTickCount64()+60000;do { enum xide_c_job_state s=xideCCompletionJobState(j,message,768);if(s==wanted)return 1;if(s>=XIDE_C_JOB_FAILED && s!=wanted)return 0;Sleep(5); }while(GetTickCount64()<until);return 0; }
static xvalue* query(xide_c_completion_job* j,enum xide_c_query kind,int offset) { uint64_t token=xideCAnalysisJobRequest(j,kind,501,offset),until=GetTickCount64()+30000;do { xide_c_event e={0};if(xideCAnalysisJobTake(j,token,&e)) { xvalue* v=e.message;e.message=NULL;xideCEventClear(&e);return v; }Sleep(5); }while(token && GetTickCount64()<until);xideCAnalysisJobCancelRequest(j,token);return NULL; }
#define CHECK(v) do { ++checks;if(!(v)) { fprintf(stderr,"extension probe line %d: %s; %s; %s\n",__LINE__,#v,error.message,message);failed=1;goto cleanup; } } while(0)
int main(int argc,char** argv)
{
 if(argc!=5)return 2;
 xide_model_error error={0};char message[768]={0};unsigned char *raw=NULL,*c=NULL,*xl=NULL,*original=NULL;size_t n=0,cBytes=0,xlBytes=0,originalBytes=0;
 xvalue *config=NULL,*projects=NULL,*database=NULL,*p=NULL,*row=NULL,*args=NULL,*reply=NULL,*mapped=NULL;char* relative=NULL;xide_c_completion_job* job=NULL;int changed=0;
 CHECK(xrtDirCreateAll(argv[4]));raw=xrtFileReadAllLimit(argv[1],65536,&n);CHECK(raw);config=xrtJsonParse((xstrview){(char*)raw,n});xrtFree(raw);raw=NULL;CHECK(config);
 const char *root=text(get(config,"ownedRoot")),*directory=text(get(config,"directory")),*entry=text(get(config,"entry")),*native=text(get(config,"native")),*guard=text(get(config,"guard"));
 c=xrtFileReadAllLimit(native,1024*1024,&cBytes);xl=xrtFileReadAllLimit(entry,1024*1024,&xlBytes);CHECK(c && xl && !memchr(c,0,cBytes) && !memchr(xl,0,xlBytes));
 /* ReadAll supplies a trailing NUL; all test sources are ordinary UTF-8. */
 projects=xrtValueArray();database=xrtValueArray();p=xrtValueObject();row=xrtValueObject();args=xrtValueArray();CHECK(projects && database && p && row && args);
 CHECK(put(p,"directory",xrtValueString(cjsv(directory))) && put(p,"entry",xrtValueString(cjsv(entry))) && put(p,"shared",xrtValueBool(false)) && put(p,"includes",xrtValueArray()) && put(p,"files",xrtValueArray()) && put(p,"cSources",xrtValueArray()));
 CHECK(xrtValueArrayAppendNew(get(p,"files"),xrtValueString(cjsv(native))) && xrtValueArrayAppendNew(get(p,"cSources"),xrtValueString(cjsv(native))) && xrtValueArrayAppend(projects,p));
 const char* arguments[]={"clang","-x","c","-std=c11","-target","x86_64-w64-windows-gnu","-c",native};
 for(size_t i=0;i<sizeof(arguments)/sizeof(*arguments);++i)CHECK(xrtValueArrayAppendNew(args,xrtValueString(cjsv(arguments[i]))));
 CHECK(put(row,"directory",xrtValueString(cjsv(directory))) && put(row,"file",xrtValueString(cjsv(native))) && xrtValueObjectSet(row,cjsv("arguments"),args) && xrtValueArrayAppend(database,row));
 xide_language_snapshot draft={entry,(char*)xl,9};xide_c_document doc={501,native,(char*)c,17};xide_c_bridge_spec bridge={argv[3],projects,&draft,1};
 xide_c_job_spec spec={.server=argv[2],.directory=directory,.commands=database,.documents=&doc,.count=1,.documentId=501,.bridge=&bridge};
 job=xideCAnalysisJobStart(&spec,&error);CHECK(job && state(job,XIDE_C_JOB_READY,message));CHECK(save(argv[4],"prepared-commands.json",job->commands));
 uint64_t sequence=0,until=GetTickCount64()+45000;int diagnosed=0;
 while(!diagnosed && GetTickCount64()<until) {
  xide_c_event events[4]={0};size_t count=0;
  if(xideCAnalysisJobDiagnostics(job,&sequence,events,4,&count))for(size_t i=0;i<count;++i) {
   char name[80];snprintf(name,sizeof(name),"diagnostics-%llu.json",(unsigned long long)sequence);int okay=save(argv[4],name,events[i].message);const xvalue* values=get(get(events[i].message,"params"),"diagnostics");
   if(!values || xrtValueCount(values)) { size_t size=0;char* json=xrtJsonStringify(events[i].message,true,&size);fprintf(stderr,"%s\n",json ? json : "missing diagnostics");xrtFree(json);okay=0; }
   xideCEventClear(&events[i]);CHECK(okay);diagnosed=1;
  }
  Sleep(5);
 }
 CHECK(diagnosed);const char* probe=strstr((char*)c,text(get(config,"probe")));CHECK(probe);
 reply=query(job,XIDE_C_HOVER,(int)(probe-(char*)c)+3);CHECK(reply && !get(reply,"error") && save(argv[4],"hover.json",reply));
 CHECK(strstr(text(get(get(get(reply,"result"),"contents"),"value")),text(get(config,"hover"))));xrtValueRelease(reply);reply=NULL;
 reply=query(job,XIDE_C_DEFINITION,(int)(probe-(char*)c)+3);CHECK(reply && !get(reply,"error") && save(argv[4],"definition.json",reply) && get(reply,"result") && xrtValueCount(get(reply,"result")));mapped=xideCBridgeDefinition(job->bridge,native,reply);CHECK(mapped && xidePathLexicalEqual(text(get(mapped,"path")),text(get(config,"definition"))) && save(argv[4],"mapped-definition.json",mapped));xrtValueRelease(mapped);mapped=NULL;xrtValueRelease(reply);reply=NULL;
 reply=query(job,XIDE_C_COMPLETION,(int)(probe-(char*)c)+(int)strlen(text(get(config,"prefix"))));CHECK(reply && !get(reply,"error") && save(argv[4],"completion.json",reply));
 const xvalue* completion=get(reply,"result"),*items=xrtValueType(completion)==XVALUE_ARRAY ? completion : get(completion,"items");int found=0;
 for(size_t i=0;i<xrtValueCount(items);++i)if(!strcmp(text(get(xrtValueArrayGet(items,i),"filterText")),text(get(config,"completion"))))found=1;
 CHECK(found);xrtValueRelease(reply);reply=NULL;
 xide_file_snapshot snapshot={0};CHECK(xideCAnalysisJobGuard(job,guard,&snapshot));
 relative=xidePathRelative(root,guard);CHECK(relative && *relative && strncmp(relative,"..",2) && !strchr(relative,':'));
 original=xrtFileReadAllLimit(guard,1024*1024,&originalBytes);CHECK(original && originalBytes);
 unsigned char* different=malloc(originalBytes);CHECK(different);memcpy(different,original,originalBytes);different[originalBytes-1]^=1;
 changed=xrtFileWriteAtomic(guard,(xbytesview){different,originalBytes});free(different);CHECK(changed && state(job,XIDE_C_JOB_INVALIDATED,message));
 CHECK(!xideCAnalysisJobRequest(job,XIDE_C_HOVER,501,0) && !xideCCompletionJobResult(job));
 CHECK(xrtFileWriteAtomic(guard,(xbytesview){original,originalBytes}));changed=0;
 printf("Actual extension IDE context passed: %d checks; real diagnostics, hover, definition, completion and imported-header invalidation.\n",checks);
cleanup:
 if(changed && original && config)xrtFileWriteAtomic(text(get(config,"guard")),(xbytesview){original,originalBytes});
 xideCCompletionJobDestroy(job);xrtFree(raw);xrtFree(c);xrtFree(xl);xrtFree(original);free(relative);
 xrtValueRelease(config);xrtValueRelease(projects);xrtValueRelease(database);xrtValueRelease(p);xrtValueRelease(row);xrtValueRelease(args);xrtValueRelease(reply);xrtValueRelease(mapped);return failed;
}
