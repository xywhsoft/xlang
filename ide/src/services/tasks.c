#include "services/tasks.h"
#include "services/resource_inputs.h"
#include "services/build_inputs.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "platform/task_stage.h"
#include <windows.h>
#include <objbase.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#define LOG_LIMIT (1024u*1024u)
#define RING_INITIAL 4096u
#define INPUT_FILE_LIMIT (16u*1024u*1024u)
#define INPUT_TOTAL_LIMIT (128u*1024u*1024u)
typedef struct ring { unsigned char* bytes; size_t start,count,capacity; uint64_t base; } ring;
typedef struct input_file { char* path; unsigned char* baseline; size_t size; int required,reportedOnly; } input_file;
struct xide_task {
    CRITICAL_SECTION lock;
    HANDLE worker;
    xide_process* process;
    xide_task_info info;
    char *projectId,*configurationId,*name,*manifest,*kind,*entry,*compiler,*directory,*workingDirectory,*output;
    char *stageDirectory,*artifact,*snapshot,*probeLog,*buildLog,*runLog,*promotion,*testProgram,*testLog;
    char *receipt,*invocation,*packagePath;
    xide_build_inputs* buildInputs;
    xide_strings cSources,resources,includePaths,libraryPaths,linkLibraries,arguments,changedPaths,pendingInputs;
    xprocessenv* environment; size_t environmentCount;
    xvalue* source; xide_toolchains* toolchains; xide_toolchain_profile toolchain;
    input_file* inputs; size_t inputCount,inputCapacity;
    xide_diagnostic* diagnostics; size_t diagnosticCount,diagnosticCapacity;
    ring log,terminal;
    xide_task_stage* stage;
    xide_task_stage** stageReferences; size_t stageReferenceCount,stageReferenceCapacity;
    int maintainCache;
    int run,test,cancel,invalidated,requestReleased;
    uint64_t inputRevision,checkedInputRevision; size_t checkedChanges;
    ULONGLONG began;
};
struct xide_tasks { xide_toolchains* toolchains; xide_task** items; size_t count,capacity; uint64_t nextId; xide_task* active; char* maintenanceRoots[64]; ULONGLONG maintenanceTimes[64]; size_t maintenanceCount; };
static char* copy(const char* text)
{
    if(!text) return NULL;
    size_t size=strlen(text)+1; char* value=malloc(size); if(value) memcpy(value,text,size); return value;
}
static int fail(xide_model_error* error,const char* format,...)
{
    if(error) { error->code=XIDE_MODEL_INVALID; va_list args; va_start(args,format); vsnprintf(error->message,sizeof(error->message),format,args); va_end(args); }
    return 0;
}
static int stringsCopy(xide_strings* target,const xide_strings* source)
{
    target->items=calloc(source->count+1,sizeof(*target->items)); if(!target->items) return 0;
    target->count=source->count; target->capacity=source->count+1;
    for(size_t i=0;i<source->count;++i) if(!(target->items[i]=copy(source->items[i]))) return 0;
    return 1;
}
static void stringsClear(xide_strings* strings)
{
    for(size_t i=0;i<strings->count;++i) free(strings->items[i]);
    free(strings->items);
    memset(strings,0,sizeof(*strings));
}
static void ringResize(ring* target,size_t capacity)
{
    unsigned char* bytes=malloc(capacity); if(!bytes) return;
    size_t first=target->capacity-target->start; if(first>target->count) first=target->count;
    memcpy(bytes,target->bytes+target->start,first); memcpy(bytes+first,target->bytes,target->count-first);
    free(target->bytes); target->bytes=bytes; target->capacity=capacity; target->start=0;
}
static void ringAppend(ring* target,const void* data,size_t size)
{
    const unsigned char* bytes=data;
    if(size>LOG_LIMIT) { target->base+=target->count+size-LOG_LIMIT; target->start=0; target->count=0; bytes+=size-LOG_LIMIT; size=LOG_LIMIT; }
    size_t wanted=target->count+size; if(wanted>LOG_LIMIT) wanted=LOG_LIMIT;
    if(wanted>target->capacity) { size_t capacity=target->capacity; while(capacity<wanted) capacity*=2; ringResize(target,capacity); }
    /* On allocation failure the smaller ring still retains the newest bytes
       and reports their absolute cursor gap; the raw process log stays complete. */
    if(size>target->capacity) { target->base+=target->count+size-target->capacity; target->start=target->count=0; bytes+=size-target->capacity; size=target->capacity; }
    size_t drop=target->count+size>target->capacity ? target->count+size-target->capacity : 0;
    target->start=(target->start+drop)%target->capacity; target->count-=drop; target->base+=drop;
    size_t end=(target->start+target->count)%target->capacity,first=target->capacity-end; if(first>size) first=size;
    memcpy(target->bytes+end,bytes,first); memcpy(target->bytes,bytes+first,size-first); target->count+=size;
}
static void logText(xide_task* task,const char* format,...)
{
    char message[2048]; va_list args; va_start(args,format); vsnprintf(message,sizeof(message),format,args); va_end(args);
    EnterCriticalSection(&task->lock); ringAppend(&task->log,message,strlen(message)); ++task->info.revision; LeaveCriticalSection(&task->lock);
}
static int interrupted(xide_task* task)
{
    EnterCriticalSection(&task->lock);
    int result=task->cancel || task->invalidated;
    size_t start=task->checkedInputRevision==task->inputRevision ? task->checkedChanges : 0,count=task->changedPaths.count;
    char** changes=!result && count>start ? malloc((count-start)*sizeof(*changes)) : NULL;
    if(!result && count>start && !changes) { task->invalidated=1; result=1; }
    if(changes) memcpy(changes,task->changedPaths.items+start,(count-start)*sizeof(*changes));
    /* Retain all notifications. New graph paths must retry even notifications
       that did not match any previously known direct/resource input. */
    task->checkedChanges=count; task->checkedInputRevision=task->inputRevision;
    LeaveCriticalSection(&task->lock);
    for(size_t i=0;changes && i<count-start && !result;++i) for(size_t j=0;j<task->inputCount;++j) {
        if(xidePathLexicalEqual(changes[i],task->inputs[j].path) || xidePathEqual(changes[i],task->inputs[j].path)) { result=1; EnterCriticalSection(&task->lock); task->invalidated=1; ++task->info.revision; LeaveCriticalSection(&task->lock); break; }
    }
    free(changes);
    return result;
}
static void setPhase(xide_task* task,enum xide_task_phase phase)
{
    EnterCriticalSection(&task->lock); task->info.phase=phase; ++task->info.revision; LeaveCriticalSection(&task->lock);
}
static void setFailure(xide_task* task,const char* format,...)
{
    EnterCriticalSection(&task->lock);
    va_list args; va_start(args,format); vsnprintf(task->info.error,sizeof(task->info.error),format,args); va_end(args);
    ++task->info.revision; LeaveCriticalSection(&task->lock);
}
/* Worker-only: retain input-stage leases through history, including paths
   subsequently discovered from resource scans and compiler receipts. */
static int protectStageInput(xide_task* task,const char* path)
{
    if(!path || !*path) return 1;
    xide_model_error error={0}; xide_task_stage* reference=xideTaskStageProtectPath(path,&error);
    if(!reference) { if(error.code) setFailure(task,"Cannot protect task input %s: %s",path,error.message); return !error.code; }
    const char* directory=xideTaskStageDirectory(reference);
    for(size_t i=0;i<task->stageReferenceCount;++i) if(xidePathLexicalEqual(directory,xideTaskStageDirectory(task->stageReferences[i]))) { xideTaskStageRelease(reference); return 1; }
    if(task->stageReferenceCount==task->stageReferenceCapacity) {
        size_t count=task->stageReferenceCapacity ? task->stageReferenceCapacity*2 : 4;
        xide_task_stage** refs=realloc(task->stageReferences,count*sizeof(*refs));
        if(!refs) { xideTaskStageRelease(reference); setFailure(task,"Cannot retain referenced task stage protection"); return 0; }
        task->stageReferences=refs; task->stageReferenceCapacity=count;
    }
    task->stageReferences[task->stageReferenceCount++]=reference; return 1;
}
static int protectRequestStages(xide_task* task)
{
    const char* paths[]={task->compiler,task->testProgram,task->directory,task->workingDirectory,task->output};
    for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) if(!protectStageInput(task,paths[i])) return 0;
    const xide_strings* lists[]={&task->includePaths,&task->libraryPaths,&task->pendingInputs};
    for(size_t i=0;i<sizeof(lists)/sizeof(*lists);++i) for(size_t j=0;j<lists[i]->count;++j) if(!protectStageInput(task,lists[i]->items[j])) return 0;
    if(task->packagePath && *task->packagePath) {
        char* list=copy(task->packagePath); if(!list) { setFailure(task,"Cannot retain package path protection"); return 0; }
        int okay=1; char* next=list;
        while(next && okay) {
            char* end=strchr(next,';'); if(end) *end=0;
            if(*next) {
                /* The compiler runs in the project directory. Resolve only the
                   lease lookup; its captured environment and receipt stay raw. */
                char* directory=xidePathResolveLexical(task->directory,next);
                if(!directory) { setFailure(task,"Cannot resolve package path protection"); okay=0; }
                else { okay=protectStageInput(task,directory); free(directory); }
            }
            next=end ? end+1 : NULL;
        }
        free(list); if(!okay) return 0;
    }
    return 1;
}
static int addInput(xide_task* task,const char* path,int required)
{
    EnterCriticalSection(&task->lock);
    for(size_t i=0;i<task->inputCount;++i) if(!strcmp(task->inputs[i].path,path)) { task->inputs[i].required|=required>0; LeaveCriticalSection(&task->lock); return 1; }
    if(task->inputCount==task->inputCapacity) {
        size_t capacity=task->inputCapacity ? task->inputCapacity*2 : 16;
        input_file* inputs=realloc(task->inputs,capacity*sizeof(*inputs)); if(!inputs) { LeaveCriticalSection(&task->lock); return 0; }
        task->inputs=inputs; task->inputCapacity=capacity;
    }
    char* value=copy(path); if(!value) { LeaveCriticalSection(&task->lock); return 0; }
    task->inputs[task->inputCount++]=(input_file){.path=value,.required=required>0,.reportedOnly=required<0};
    ++task->inputRevision;
    ++task->info.revision;
    LeaveCriticalSection(&task->lock); return 1;
}
static int captureInputs(xide_task* task,int compare)
{
    size_t total=0;
    for(size_t i=0;i<task->inputCount;++i) {
        if(interrupted(task)) return 0;
        input_file* input=&task->inputs[i]; size_t size=0;
        if(!protectStageInput(task,input->path)) return 0;
        if(input->reportedOnly) continue;
        if(!compare && input->baseline) { total+=input->size; continue; }
        if(!input->required && !input->baseline && !xidePathExists(input->path)) continue;
        unsigned char* bytes=xideIoReadAllLimit(input->path,INPUT_FILE_LIMIT,&size);
        if(!bytes) { setFailure(task,"Unable to read build input: %s",input->path); return 0; }
        if(compare) {
            int equal=input->baseline && size==input->size && !memcmp(bytes,input->baseline,size); xrtFree(bytes);
            if(!equal) { setFailure(task,"Build input changed while the task was running: %s",input->path); return 0; }
        } else { if(size>INPUT_TOTAL_LIMIT-total) { xrtFree(bytes); setFailure(task,"Build input snapshots exceed the current 128 MiB budget"); return 0; } total+=size; input->baseline=bytes; input->size=size; }
    }
    return 1;
}
static int stringsEqual(const xide_strings* a,const xide_strings* b)
{
    size_t count=b ? b->count : 0;
    if(a->count!=count) return 0;
    for(size_t i=0;i<count;++i) if(strcmp(a->items[i],b->items[i])) return 0;
    return 1;
}
static int captureResourceInputs(xide_task* task)
{
    for(size_t i=0;i<task->resources.count;++i) {
        if(interrupted(task)) return 0;
        const input_file* config=NULL;
        for(size_t j=0;j<task->inputCount;++j) if(!strcmp(task->resources.items[i],task->inputs[j].path)) { config=&task->inputs[j]; break; }
        xide_strings paths={0}; xide_model_error error={0};
        if(!config || !config->baseline || !xideResourceInputPaths(config->baseline,config->size,task->resources.items[i],&paths,&error)) {
            setFailure(task,"Resource input scan failed: %s: %s",task->resources.items[i],error.message); return 0;
        }
        for(size_t j=0;j<paths.count;++j) {
            if(xidePathEqual(paths.items[j],task->output)) {
                setFailure(task,"Output would overwrite a resource asset: %s",paths.items[j]); xideResourceInputPathsClear(&paths); return 0;
            }
            if(!addInput(task,paths.items[j],1)) { setFailure(task,"Unable to allocate resource input paths"); xideResourceInputPathsClear(&paths); return 0; }
        }
        xideResourceInputPathsClear(&paths);
    }
    if(!captureInputs(task,0)) return 0;
    return !interrupted(task) && captureInputs(task,1);
}
static xstrview view(const char* text) { return (xstrview){text,strlen(text)}; }
static int setString(xvalue* root,const char* key,const char* text) { return xrtValueObjectSetNew(root,view(key),xrtValueString(view(text))); }
static xvalue* listValue(const xide_strings* strings)
{
    xvalue* array=xrtValueArray(); if(!array) return NULL;
    for(size_t i=0;i<strings->count;++i) if(!xrtValueArrayAppendNew(array,xrtValueString(view(strings->items[i])))) { xrtValueRelease(array); return NULL; }
    return array;
}
static int snapshotManifest(xide_task* task)
{
    xvalue* root=xrtValueClone(task->source);
    int okay=root && setString(root,"format","xlang-project-1") && setString(root,"entry",task->entry) && setString(root,"kind",task->kind) &&
        xrtValueObjectSetNew(root,view("cSources"),listValue(&task->cSources)) && xrtValueObjectSetNew(root,view("resources"),listValue(&task->resources));
    size_t size=0; char* json=okay ? xrtJsonStringify(root,true,&size) : NULL;
    okay=json && xideIoWriteAtomic(task->snapshot,(xbytesview){(unsigned char*)json,size});
    xrtFree(json); xrtValueRelease(root); if(!okay) setFailure(task,"Unable to create compiler project snapshot"); return okay;
}
static void parseDiagnostic(xide_task* task,const char* line)
{
    /* Parse the numeric location after the path, retaining Windows drive colons.
       Columns remain compiler coordinates; byte/character conversion is a UI job. */
    for(const char* colon=strchr(line,':');colon;colon=strchr(colon+1,':')) {
        if(!isdigit((unsigned char)colon[1])) continue;
        char* end=NULL; long number=strtol(colon+1,&end,10);
        if(number<1 || number>INT_MAX || *end!=':') continue;
        int column=1; const char* message=end+1;
        if(isdigit((unsigned char)*message)) { long value=strtol(message,&end,10); if(value<1 || value>INT_MAX || *end!=':') continue; column=(int)value; message=end+1; }
        while(*message==' ') ++message;
        size_t pathSize=(size_t)(colon-line); if(!pathSize || !*message) return;
        char* path=malloc(pathSize+1); if(!path) return; memcpy(path,line,pathSize); path[pathSize]=0;
        char* absolute=xidePathResolve(task->directory,path); free(path);
        char* text=copy(message); if(!absolute || !text) { free(absolute); free(text); return; }
        EnterCriticalSection(&task->lock);
        if(task->diagnosticCount==task->diagnosticCapacity) {
            size_t capacity=task->diagnosticCapacity ? task->diagnosticCapacity*2 : 16;
            xide_diagnostic* diagnostics=realloc(task->diagnostics,capacity*sizeof(*diagnostics));
            if(!diagnostics) { LeaveCriticalSection(&task->lock); free(absolute); free(text); return; }
            task->diagnostics=diagnostics; task->diagnosticCapacity=capacity;
        }
        task->diagnostics[task->diagnosticCount++]=(xide_diagnostic){absolute,text,(int)number,column,XIDE_DIAGNOSTIC_COMPILER};
        ++task->info.revision; LeaveCriticalSection(&task->lock); return;
    }
}
static int runProcess(xide_task* task,const xide_process_spec* request,enum xide_task_phase phase,char* probe,size_t probeCapacity)
{
    if(interrupted(task)) return 0;
    setPhase(task,phase); char error[512]={0};
    xide_process* process=xideProcessStart(request,error,sizeof(error));
    if(!process) { setFailure(task,"%s",error); return 0; }
    EnterCriticalSection(&task->lock); task->process=process; LeaveCriticalSection(&task->lock);
    char bytes[16384],line[65536]; size_t lineSize=0,probeSize=0; uint64_t dropped=0; xide_process_info info={0};
    int finished=0; uint64_t processBegan=GetTickCount64();
    while(!finished) {
        if(interrupted(task)) xideProcessCancel(process);
        if(phase==XIDE_TASK_PROBE && GetTickCount64()-processBegan>15000u) { setFailure(task,"Compiler help query exceeded the 15 second limit"); xideProcessCancel(process); }
        size_t size=xideProcessRead(process,bytes,sizeof(bytes));
        if(size) {
            if(phase==XIDE_TASK_PROBE) {
                if(memchr(bytes,0,size)) { setFailure(task,"Compiler help contains NUL bytes"); xideProcessCancel(process); }
                size_t available=probeCapacity>probeSize+1 ? probeCapacity-probeSize-1 : 0;
                if(size>available) { setFailure(task,"Compiler help exceeds the 64 KiB query limit"); xideProcessCancel(process); }
                size_t count=size<available ? size : available; if(count) { memcpy(probe+probeSize,bytes,count); probeSize+=count; probe[probeSize]=0; }
            } else {
                EnterCriticalSection(&task->lock); ringAppend(&task->log,bytes,size); if(phase==XIDE_TASK_RUN || phase==XIDE_TASK_TEST) ringAppend(&task->terminal,bytes,size); ++task->info.revision; LeaveCriticalSection(&task->lock);
            }
            if(phase==XIDE_TASK_BUILD) for(size_t i=0;i<size;++i) {
                if(bytes[i]=='\n') { line[lineSize]=0; if(lineSize && line[lineSize-1]=='\r') line[lineSize-1]=0; parseDiagnostic(task,line); lineSize=0; }
                else if(lineSize+1<sizeof(line)) line[lineSize++]=bytes[i];
            }
        }
        xideProcessGetInfo(process,&info);
        if(phase==XIDE_TASK_RUN && info.started) { EnterCriticalSection(&task->lock); task->info.ran=1; LeaveCriticalSection(&task->lock); }
        if(phase==XIDE_TASK_TEST && info.started) { EnterCriticalSection(&task->lock); task->info.tested=1; LeaveCriticalSection(&task->lock); }
        /* FINISHED is published after the final pipe read; drain queued bytes. */
        if(info.state==XIDE_PROCESS_FINISHED && !size) finished=1;
        if(info.droppedBytes>dropped) { logText(task,"\n[Output queue omitted %llu bytes; see raw log]\n",(unsigned long long)(info.droppedBytes-dropped)); dropped=info.droppedBytes; }
        if(!size && !finished) Sleep(5);
    }
    if(phase==XIDE_TASK_BUILD && lineSize) { line[lineSize]=0; parseDiagnostic(task,line); }
    EnterCriticalSection(&task->lock); task->process=NULL; task->info.exit=info.exit; ++task->info.revision; LeaveCriticalSection(&task->lock);
    xideProcessDestroy(process);
    if(interrupted(task)) return 0;
    if(phase==XIDE_TASK_PROBE && (*task->info.error || info.droppedBytes)) { if(!*task->info.error) setFailure(task,"Compiler help output was incomplete"); return 0; }
    if(info.ioError || !info.started) { setFailure(task,"%s",info.error); return 0; }
    if(info.exit.Kind!=XPROCESS_EXIT_CODE || info.exit.Code!=0) { setFailure(task,"%s exited with code %d",phase==XIDE_TASK_TEST ? "Test program" : phase==XIDE_TASK_RUN ? "Program" : "Compiler",info.exit.Code); return 0; }
    return 1;
}
static int receiptCancelled(void* user) { return interrupted(user); }
static int taskOutputValid(xide_task* task,const char* output)
{
    for(size_t i=0;i<task->inputCount;++i) if(xidePathLexicalEqual(task->inputs[i].path,output) || xidePathEqual(task->inputs[i].path,output)) { setFailure(task,"Output would overwrite a task input or candidate: %s",task->inputs[i].path); return 0; }
    char* directory=xidePathDirectory(task->compiler); const char* names[]={"xlc.dll","xl.dll"};
    if(!directory) { setFailure(task,"Unable to inspect compiler publication paths"); return 0; }
    int okay=!xidePathLexicalEqual(task->compiler,output) && !xidePathEqual(task->compiler,output);
    for(size_t i=0;i<2 && okay;++i) { char* path=xidePathResolveLexical(directory,names[i]); okay=path && !xidePathLexicalEqual(path,output) && !xidePathEqual(path,output); free(path); }
    free(directory); if(!okay) setFailure(task,"Output would overwrite the compiler or a sibling runtime");
    if(okay && task->buildInputs) { xide_model_error e={0}; okay=xideBuildInputsOutputValid(task->buildInputs,output,&e); if(!okay) setFailure(task,"%s",e.message); }
    return okay;
}
static int snapshotInvocation(xide_task* task,const char** arguments,size_t count)
{
    xvalue* root=xrtValueObject(); xide_strings list={.items=(char**)arguments,.count=count};
    int okay=root && setString(root,"format","xlang-ide-build-invocation-1") && setString(root,"compiler",task->compiler) && setString(root,"directory",task->directory) && setString(root,"artifact",task->artifact) && setString(root,"receipt",task->receipt) &&
        xrtValueObjectSetNew(root,view("arguments"),listValue(&list)) && xrtValueObjectSetNew(root,view("XLANG_PACKAGE_PATH"),task->packagePath ? xrtValueString(view(task->packagePath)) : xrtValueNull());
    const char* names[]={"compilerSha256","frontendSha256","runtimeSha256"};
    for(int i=0;okay && i<3;++i) {
        char hash[65]; for(int j=0;j<32;++j) snprintf(hash+2*j,3,"%02x",task->toolchain.stamp.files[i].sha256[j]);
        okay=xrtValueObjectSetNew(root,view(names[i]),task->toolchain.stamp.present[i] ? xrtValueString(view(hash)) : xrtValueNull());
    }
    size_t size=0; char* json=okay ? xrtJsonStringify(root,true,&size) : NULL;
    okay=json && xideIoWriteAtomic(task->invocation,(xbytesview){(unsigned char*)json,size}); xrtFree(json); xrtValueRelease(root);
    if(!okay) { setFailure(task,"Unable to retain the actual compiler invocation"); } return okay;
}
static int loadBuildInputs(xide_task* task)
{
    xide_model_error e={0};
    task->buildInputs=xideBuildInputsLoad(task->receipt,task->artifact,task->packagePath,receiptCancelled,task,&e);
    if(!task->buildInputs) { setFailure(task,"%s",e.message); return 0; }
    if(!xideBuildInputsHasFile(task->buildInputs,task->snapshot) || !xideBuildInputsHasFile(task->buildInputs,task->entry)) { setFailure(task,"Compiler receipt omits this task's project snapshot or entry"); return 0; }
    const xide_strings* required[]={&task->cSources,&task->resources};
    for(size_t i=0;i<2;++i) for(size_t j=0;j<required[i]->count;++j) if(!xideBuildInputsHasFile(task->buildInputs,required[i]->items[j])) { setFailure(task,"Compiler receipt omits a configured build input: %s",required[i]->items[j]); return 0; }
    size_t count=xideBuildInputsPathCount(task->buildInputs);
    for(size_t i=0;i<count;++i) {
        EnterCriticalSection(&task->lock); int stopped=task->cancel || task->invalidated; LeaveCriticalSection(&task->lock);
        if(stopped) return 0;
        const char* path=xideBuildInputsPath(task->buildInputs,i);
        if(!protectStageInput(task,path)) return 0;
        if(!addInput(task,path,-1)) { setFailure(task,"Unable to retain compiler input paths"); return 0; }
    }
    logText(task,"[Compiler inputs verified] %llu file/candidate paths\nInput receipt: %s\n",(unsigned long long)count,task->receipt);
    return !interrupted(task);
}
static int validateBuildInputs(xide_task* task)
{
    xide_model_error e={0};
    if(!captureInputs(task,1) || interrupted(task)) return 0;
    if(task->buildInputs && !xideBuildInputsValidate(task->buildInputs,&e)) { setFailure(task,"%s",e.message); return 0; }
    if(!xideToolchainValidate(task->compiler,&task->toolchain.stamp,&e)) { setFailure(task,"%s",e.message); return 0; }
    return !interrupted(task);
}
/* Worker-only: report the request's named dirty inputs against the paths actually
   discovered by resource scanning or the same build's compiler receipt. Keep the
   editor spelling so a physical alias reopens the existing shared document. */
static size_t reportPendingInputs(xide_task* task)
{
    EnterCriticalSection(&task->lock); int report=task->invalidated && !task->cancel; LeaveCriticalSection(&task->lock);
    if(!report) return 0;
    size_t added=0;
    for(size_t i=0;i<task->pendingInputs.count;++i) {
        const char* path=task->pendingInputs.items[i]; int matched=0,duplicate=0;
        for(size_t j=0;j<task->inputCount && !matched;++j)
            matched=xidePathLexicalEqual(path,task->inputs[j].path) || xidePathEqual(path,task->inputs[j].path);
        for(size_t j=0;j<xideBuildInputsPathCount(task->buildInputs) && !matched;++j) {
            const char* observed=xideBuildInputsPath(task->buildInputs,j);
            matched=xidePathLexicalEqual(path,observed) || xidePathEqual(path,observed);
        }
        if(!matched) continue;
        for(size_t j=0;j<task->diagnosticCount && !duplicate;++j) if(task->diagnostics[j].kind==XIDE_DIAGNOSTIC_UNSAVED_INPUT)
            duplicate=xidePathLexicalEqual(path,task->diagnostics[j].path) || xidePathEqual(path,task->diagnostics[j].path);
        if(duplicate) continue;
        char* owned=copy(path); char* message=copy("关联输入在任务提交时有未保存修改；保存后重新构建。");
        if(!owned || !message) { free(owned); free(message); setFailure(task,"Unable to retain all unsaved input notices"); break; }
        EnterCriticalSection(&task->lock);
        if(task->diagnosticCount==task->diagnosticCapacity) {
            size_t capacity=task->diagnosticCapacity ? task->diagnosticCapacity*2 : 16;
            xide_diagnostic* rows=realloc(task->diagnostics,capacity*sizeof(*rows));
            if(!rows) { LeaveCriticalSection(&task->lock); free(owned); free(message); setFailure(task,"Unable to retain all unsaved input notices"); break; }
            task->diagnostics=rows; task->diagnosticCapacity=capacity;
        }
        task->diagnostics[task->diagnosticCount++]=(xide_diagnostic){owned,message,0,0,XIDE_DIAGNOSTIC_UNSAVED_INPUT};
        ++task->info.revision; LeaveCriticalSection(&task->lock); ++added;
        logText(task,"[Unsaved input at submission] %s\n",path);
    }
    return added;
}
typedef struct maintenance_cancel { xide_task* task; ULONGLONG deadline; } maintenance_cancel;
static int maintenanceCancelled(void* user)
{ maintenance_cancel* state=user; return GetTickCount64()>state->deadline || interrupted(state->task); }
static void finishStage(xide_task* task)
{
    if(!task->stage) return;
    xide_model_error error={0};
    if(!xideTaskStageComplete(task->stage,&error)) logText(task,"[Task cache preserved] %s\n",error.message);
    if(!task->maintainCache) return;
    char* root=xidePathDirectory(task->stageDirectory); size_t count=task->inputCount+task->pendingInputs.count+6;
    const char** paths=calloc(count,sizeof(*paths));
    if(!root || !paths) { free(root); free(paths); logText(task,"[Task cache preserved] Unable to allocate maintenance protection.\n"); return; }
    size_t used=0; for(size_t i=0;i<task->inputCount;++i) paths[used++]=task->inputs[i].path;
    for(size_t i=0;i<task->pendingInputs.count;++i) paths[used++]=task->pendingInputs.items[i];
    paths[used++]=task->workingDirectory; paths[used++]=task->compiler; paths[used++]=task->testProgram; paths[used++]=task->output; paths[used++]=task->stageDirectory; paths[used++]=task->directory;
    xide_strings protected={.items=(char**)paths,.count=used}; FILETIME time={0}; GetSystemTimeAsFileTime(&time);
    uint64_t now=((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime; xide_task_stage_stats stats={0}; maintenance_cancel cancel={task,GetTickCount64()+5000};
    int okay=xideTaskStagePrune(root,XIDE_TASK_HISTORY_LIMIT,now,&protected,maintenanceCancelled,&cancel,&stats,&error);
    logText(task,"[Task cache] removed=%llu stages/%llu files; bytes=%llu; retained=%llu skipped=%llu%s%s\n",(unsigned long long)stats.deletedStages,(unsigned long long)stats.deletedFiles,(unsigned long long)stats.releasedBytes,(unsigned long long)stats.retained,(unsigned long long)stats.skipped,okay ? "" : "; ",okay ? "" : error.message);
    free(root); free(paths);
}
static DWORD WINAPI taskWorker(void* data)
{
    xide_task* task=data; char probe[65536]={0}; int success=0; size_t pendingNotices=0;
    if(interrupted(task) || !protectRequestStages(task)) goto finished;
    if(task->test) {
        logText(task,"[Test task %llu] %s / %s\nProgram: %s\nDirectory: %s\nRaw test log: %s\n",(unsigned long long)task->info.id,task->name,task->configurationId,task->testProgram,task->workingDirectory,task->testLog);
        xide_model_error stageError={0}; task->stage=xideTaskStageBegin(task->stageDirectory,&stageError);
        if(!task->stage || !captureInputs(task,0)) { if(!*task->info.error) setFailure(task,"%s",task->stage ? "Unable to prepare test task inputs/log directory" : stageError.message); goto finished; }
        input_file* manifest=&task->inputs[0]; size_t start=manifest->size>=3 && !memcmp(manifest->baseline,"\xef\xbb\xbf",3) ? 3 : 0;
        xvalue* current=xrtJsonParse((xstrview){(char*)manifest->baseline+start,manifest->size-start});
        int same=current && xrtValueEqual(current,task->source); xrtValueRelease(current);
        if(!same) { setFailure(task,"Project manifest changed outside the loaded model; reopen it before testing"); goto finished; }
        if(!snapshotManifest(task) || !captureInputs(task,1) || interrupted(task)) goto finished;
        xide_process_spec request={.program=task->testProgram,.directory=task->workingDirectory,.arguments=(const char* const*)task->arguments.items,.argumentCount=task->arguments.count,
            .environment=task->environment,.environmentCount=task->environmentCount,.interactive=1,.logPath=task->testLog};
        success=runProcess(task,&request,XIDE_TASK_TEST,NULL,0); goto finished;
    }
    logText(task,"[Task %llu] %s / %s\nCompiler: %s\n",(unsigned long long)task->info.id,task->name,task->configurationId,task->compiler);
    /* Physical identity checks can open files. Keep the potentially large
       input scan on this worker rather than the caller/UI thread. */
    if(interrupted(task) || !taskOutputValid(task,task->output)) goto finished;
    xide_model_error stageError={0}; task->stage=xideTaskStageBegin(task->stageDirectory,&stageError);
    if(!task->stage) { setFailure(task,"%s",stageError.message); goto finished; }
    if(!captureInputs(task,0)) goto finished;
    input_file* manifest=&task->inputs[0]; size_t start=manifest->size>=3 && !memcmp(manifest->baseline,"\xef\xbb\xbf",3) ? 3 : 0;
    xvalue* current=xrtJsonParse((xstrview){(char*)manifest->baseline+start,manifest->size-start});
    int same=current && xrtValueEqual(current,task->source); xrtValueRelease(current);
    if(!same) { setFailure(task,"Project manifest changed outside the loaded model; reopen it before building"); goto finished; }
    if(!captureResourceInputs(task)) goto finished;
    if(!snapshotManifest(task)) goto finished;
    xide_toolchain_stamp stamp={0}; xide_model_error toolError={0};
    if(!xideToolchainCapture(task->compiler,&stamp,&toolError)) { setFailure(task,"%s",toolError.message); goto finished; }
    const char* helpArguments[]={"--help"};
    xide_process_spec request={.program=task->compiler,.directory=task->directory,.arguments=helpArguments,.argumentCount=1,.logPath=task->probeLog};
    if(!runProcess(task,&request,XIDE_TASK_PROBE,probe,sizeof(probe))) goto finished;
    xide_toolchain_profile observed={0};
    if(!xideToolchainObserve(task->toolchains,task->compiler,task->directory,&stamp,probe,strlen(probe),&observed,&toolError)) { setFailure(task,"%s",toolError.message); goto finished; }
    EnterCriticalSection(&task->lock); task->toolchain=observed; ++task->info.revision; LeaveCriticalSection(&task->lock);
    int report=!!(observed.options&XIDE_TOOL_BUILD_INPUT_REPORT);
    EnterCriticalSection(&task->lock); task->info.inputReport=report; LeaveCriticalSection(&task->lock);
    if(!(observed.commands&XIDE_TOOL_BUILD) || !(observed.options&XIDE_TOOL_OUTPUT)) { setFailure(task,"Configured executable does not expose the supported 'build ... -o ...' CLI"); goto finished; }
    if((task->includePaths.count && !(observed.options&XIDE_TOOL_INCLUDE)) || (task->libraryPaths.count && !(observed.options&XIDE_TOOL_LIBRARY)) || (task->linkLibraries.count && !(observed.options&XIDE_TOOL_LINK))) {
        setFailure(task,"Compiler help does not advertise the configured include/library/link options"); goto finished;
    }
    size_t count=4+2*(task->includePaths.count+task->libraryPaths.count+task->linkLibraries.count)+(report ? 2 : 0);
    const char** arguments=calloc(count+1,sizeof(*arguments)); if(!arguments) { setFailure(task,"Unable to allocate compiler arguments"); goto finished; }
    arguments[0]="build"; arguments[1]=task->snapshot; arguments[2]="-o"; arguments[3]=task->artifact; size_t cursor=4;
    const xide_strings* lists[]={&task->includePaths,&task->libraryPaths,&task->linkLibraries}; const char* options[]={"--inc-path","--lib-path","--link"};
    for(size_t i=0;i<3;++i) for(size_t j=0;j<lists[i]->count;++j) { arguments[cursor++]=options[i]; arguments[cursor++]=lists[i]->items[j]; }
    if(report) { arguments[cursor++]="--input-report"; arguments[cursor++]=task->receipt; }
    if(!snapshotInvocation(task,arguments,count)) { free(arguments); goto finished; }
    request.arguments=arguments; request.argumentCount=count; request.logPath=task->buildLog;
    xprocessenv buildEnvironment={"XLANG_PACKAGE_PATH",task->packagePath}; request.environment=&buildEnvironment; request.environmentCount=1;
    logText(task,report ? "[Build input coverage] Actual compiler disk inputs and search candidates are required.\n" : "[Build input coverage] Compiler does not advertise --input-report; only direct/project/resource guards are available.\n");
    logText(task,"[Build] %s\nRaw build log: %s\nRaw run log: %s\n",task->manifest,task->buildLog,task->runLog);
    int built=runProcess(task,&request,XIDE_TASK_BUILD,NULL,0); free(arguments);
    if(!built) goto finished;
    if(!xidePathExists(task->artifact)) { setFailure(task,"Compiler reported success without producing this task's artifact"); goto finished; }
    if(report && !loadBuildInputs(task)) goto finished;
    if(!validateBuildInputs(task) || !taskOutputValid(task,task->output) || !taskOutputValid(task,task->promotion)) goto finished;
    /* Copy to a task-owned sibling volume before atomic replacement; a locked
       destination leaves the old artifact intact and makes this task fail. */
    if(!xideIoCopy(task->artifact,task->promotion,true)) { setFailure(task,"Unable to copy the newly built artifact for publication"); goto finished; }
    if(task->buildInputs && !xideBuildInputsArtifactValid(task->buildInputs,task->promotion,&toolError)) { setFailure(task,"%s",toolError.message); goto finished; }
    if(!validateBuildInputs(task) || !taskOutputValid(task,task->output) || interrupted(task)) goto finished;
    if(!xideIoMove(task->promotion,task->output,true)) {
        if(!interrupted(task)) setFailure(task,"Unable to publish the newly built artifact: %s",task->output);
        goto finished;
    }
    EnterCriticalSection(&task->lock); task->info.built=1; task->info.inputsVerified=report; ++task->info.revision; LeaveCriticalSection(&task->lock);
    logText(task,"[Build succeeded] %s\n",task->output);
    if(task->run) {
        if(!validateBuildInputs(task)) goto finished;
        if(task->buildInputs && !xideBuildInputsArtifactValid(task->buildInputs,task->output,&toolError)) { setFailure(task,"%s",toolError.message); goto finished; }
        /* Only this serial task can publish/start its successful generation.
           Starting from the configured directory retains sibling DLL lookup. */
        request=(xide_process_spec){.program=task->output,.directory=task->workingDirectory,.arguments=(const char* const*)task->arguments.items,.argumentCount=task->arguments.count,
            .environment=task->environment,.environmentCount=task->environmentCount,.interactive=1,.logPath=task->runLog};
        logText(task,"[Run] %s\n",task->output);
        if(!runProcess(task,&request,XIDE_TASK_RUN,NULL,0)) goto finished;
    }
    success=1;
finished:
    pendingNotices=reportPendingInputs(task);
    for(size_t i=0;i<task->inputCount;++i) { xrtFree(task->inputs[i].baseline); task->inputs[i].baseline=NULL; }
    finishStage(task);
    EnterCriticalSection(&task->lock);
    if(task->invalidated && !*task->info.error) snprintf(task->info.error,sizeof(task->info.error),"%s",pendingNotices ? "构建已停止：相关输入在提交时未保存，请从问题页打开并保存后重新构建。" : "Input was edited or remains unsaved; save the input and request a new task");
    task->info.invalidated=task->invalidated; task->info.durationMs=GetTickCount64()-task->began;
    task->info.state=task->cancel ? XIDE_TASK_CANCELLED : success && !task->invalidated ? XIDE_TASK_SUCCEEDED : XIDE_TASK_FAILED;
    task->info.phase=XIDE_TASK_DONE; ++task->info.revision;
    char message[1024]; snprintf(message,sizeof(message),"[Task %s, %llu ms]%s%s\n",task->cancel ? "cancelled" : success && !task->invalidated ? "succeeded" : "failed",(unsigned long long)task->info.durationMs,*task->info.error ? " " : "",task->info.error);
    ringAppend(&task->log,message,strlen(message)); LeaveCriticalSection(&task->lock);
    return 0;
}
static void destroyTask(xide_task* task)
{
    if(!task) return;
    xideTaskCancel(task); if(task->worker) { WaitForSingleObject(task->worker,INFINITE); CloseHandle(task->worker); }
    char** strings[]={&task->projectId,&task->configurationId,&task->name,&task->manifest,&task->kind,&task->entry,&task->compiler,&task->directory,&task->workingDirectory,&task->output,
        &task->stageDirectory,&task->artifact,&task->snapshot,&task->probeLog,&task->buildLog,&task->runLog,&task->promotion,&task->testProgram,&task->testLog,&task->receipt,&task->invocation,&task->packagePath};
    for(size_t i=0;i<sizeof(strings)/sizeof(*strings);++i) free(*strings[i]);
    xide_strings* lists[]={&task->cSources,&task->resources,&task->includePaths,&task->libraryPaths,&task->linkLibraries,&task->arguments,&task->changedPaths,&task->pendingInputs};
    for(size_t i=0;i<sizeof(lists)/sizeof(*lists);++i) stringsClear(lists[i]);
    for(size_t i=0;i<task->environmentCount;++i) { free((void*)task->environment[i].Name); free((void*)task->environment[i].Value); } free(task->environment);
    for(size_t i=0;i<task->inputCount;++i) { free(task->inputs[i].path); xrtFree(task->inputs[i].baseline); } free(task->inputs);
    for(size_t i=0;i<task->diagnosticCount;++i) xideDiagnosticClear(&task->diagnostics[i]);
    free(task->diagnostics);
    xideTaskStageRelease(task->stage);
    for(size_t i=0;i<task->stageReferenceCount;++i) xideTaskStageRelease(task->stageReferences[i]);
    free(task->stageReferences);
    xideBuildInputsDestroy(task->buildInputs); xideToolchainProfileClear(&task->toolchain); xrtValueRelease(task->source); free(task->log.bytes); free(task->terminal.bytes); DeleteCriticalSection(&task->lock); free(task);
}
static int completedWorker(xide_task* task)
{
    xide_task_info info={0}; xideTaskInfo(task,&info);
    return info.state>=XIDE_TASK_SUCCEEDED && (!task->worker || WaitForSingleObject(task->worker,0)==WAIT_OBJECT_0);
}
static void retireRequest(xide_task* task)
{
    if(task->requestReleased || !completedWorker(task)) return;
    if(task->worker) { CloseHandle(task->worker); task->worker=NULL; }
    /* Only the owner releases requests after the worker has actually exited.
       Coalescing on that same owner thread can no longer observe these fields. */
    xide_strings* lists[]={&task->cSources,&task->resources,&task->includePaths,&task->libraryPaths,&task->linkLibraries,&task->arguments,&task->changedPaths,&task->pendingInputs};
    for(size_t i=0;i<sizeof(lists)/sizeof(*lists);++i) stringsClear(lists[i]);
    for(size_t i=0;i<task->environmentCount;++i) { free((void*)task->environment[i].Name); free((void*)task->environment[i].Value); }
    free(task->environment); task->environment=NULL; task->environmentCount=0;
    xrtValueRelease(task->source); task->source=NULL; xideBuildInputsDestroy(task->buildInputs); task->buildInputs=NULL;
    free(task->packagePath); task->packagePath=NULL;
    ring* rings[]={&task->log,&task->terminal};
    for(size_t i=0;i<2;++i) { size_t capacity=RING_INITIAL; while(capacity<rings[i]->count) capacity*=2; if(capacity<rings[i]->capacity) ringResize(rings[i],capacity); }
    task->requestReleased=1;
}
xide_tasks* xideTasksCreate(void) { xide_tasks* tasks=calloc(1,sizeof(*tasks)); if(tasks) { tasks->nextId=1; tasks->toolchains=xideToolchainsCreate(); if(!tasks->toolchains) { free(tasks); return NULL; } } return tasks; }
void xideTasksDestroy(xide_tasks* tasks)
{
    if(!tasks) return;
    for(size_t i=0;i<tasks->count;++i) xideTaskCancel(tasks->items[i]);
    for(size_t i=0;i<tasks->count;++i) destroyTask(tasks->items[i]);
    xideToolchainsDestroy(tasks->toolchains); free(tasks->items);
    for(size_t i=0;i<tasks->maintenanceCount;++i) free(tasks->maintenanceRoots[i]);
    free(tasks);
}
static int claimMaintenance(xide_tasks* tasks,const char* directory)
{
    ULONGLONG now=GetTickCount64(); size_t index=tasks->maintenanceCount;
    for(size_t i=0;i<tasks->maintenanceCount;++i) if(xidePathLexicalEqual(tasks->maintenanceRoots[i],directory)) {
        if(now-tasks->maintenanceTimes[i]<3600000) return 0;
        tasks->maintenanceTimes[i]=now; return 1;
    }
    char* path=copy(directory); if(!path) return 0;
    if(index>=64) { index=0; for(size_t i=1;i<64;++i) if(tasks->maintenanceTimes[i]<tasks->maintenanceTimes[index]) index=i; free(tasks->maintenanceRoots[index]); }
    else ++tasks->maintenanceCount;
    tasks->maintenanceRoots[index]=path; tasks->maintenanceTimes[index]=now; return 1;
}
static xide_task* startTask(xide_tasks* tasks,const xide_project* project,const char* configurationId,const char* compiler,int run,int test,const xide_strings* pendingInputs,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!tasks || !project || !project->available || project->dirty || (!test && (!compiler || !*compiler))) { fail(error,"A saved available project and an explicit task configuration are required"); return NULL; }
    if(run && strcmp(project->kind,"exe")) { fail(error,"A DLL project needs an explicit host and cannot be run directly"); return NULL; }
    const xide_configuration* config=NULL;
    for(size_t i=0;i<project->configurationCount;++i) if(!configurationId || !*configurationId || !strcmp(project->configurations[i].id,configurationId)) { config=&project->configurations[i]; break; }
    if(!config || (!test && (!config->output || !*config->output))) { fail(error,"Select an existing task configuration (build requires an output file)"); return NULL; }
    if(test && (!config->test.program || !*config->test.program || !xidePathHasExtension(config->test.program,".exe"))) { fail(error,"Configure an explicit .exe test program; scripts require an explicitly configured interpreter"); return NULL; }
    /* Apply newly observed dirty inputs to outstanding requests as well. Their
       identity checks stay on the worker; a completed task is never reused. */
    char* packagePath=NULL;
    if(!test && !xideBuildInputsPackagePath(&packagePath,error)) return NULL;
    if(pendingInputs) for(size_t i=0;i<pendingInputs->count;++i) xideTasksInvalidateInput(tasks,pendingInputs->items[i]);
    size_t pending=0;
    for(size_t i=0;i<tasks->count;++i) {
        xide_task* existing=tasks->items[i]; xide_task_info info={0}; xideTaskInfo(existing,&info);
        if(info.state<=XIDE_TASK_CANCELLING) ++pending;
        if(info.state<=XIDE_TASK_CANCELLING && info.state!=XIDE_TASK_CANCELLING && !info.invalidated && existing->run==!!run && existing->test==!!test && xidePathEqual(existing->manifest,project->manifest) && !strcmp(existing->configurationId,config->id) &&
            xrtValueEqual(existing->source,project->source) && stringsEqual(&existing->pendingInputs,pendingInputs) &&
            (test || xidePathEqual(existing->compiler,compiler)) &&
            (existing->packagePath && packagePath ? !strcmp(existing->packagePath,packagePath) : existing->packagePath==packagePath)) { free(packagePath); return existing; }
    }
    if(pending>=XIDE_TASK_PENDING_LIMIT) { free(packagePath); fail(error,"Task queue is full (32 outstanding requests); wait or stop an existing task"); return NULL; }
    xide_task* task=calloc(1,sizeof(*task)); if(!task) { free(packagePath); fail(error,"Unable to allocate task"); return NULL; }
    task->packagePath=packagePath;
    InitializeCriticalSection(&task->lock); task->toolchains=tasks->toolchains; task->info.id=tasks->nextId++; task->info.state=XIDE_TASK_QUEUED; task->info.phase=XIDE_TASK_PREPARE; task->info.revision=1; task->run=!!run;
    task->test=!!test; task->info.kind=test ? XIDE_TASK_KIND_TEST : run ? XIDE_TASK_KIND_RUN : XIDE_TASK_KIND_BUILD;
    task->projectId=copy(project->id ? project->id : ""); task->configurationId=copy(config->id); task->name=copy(project->name); task->manifest=copy(project->manifest); task->kind=copy(project->kind);
    task->entry=copy(project->entry); task->compiler=test ? copy("") : xidePathResolve(NULL,compiler); task->directory=xidePathDirectory(project->manifest); task->workingDirectory=copy(test ? config->test.workingDirectory : config->workingDirectory); task->output=copy(test ? "" : config->output);
    task->testProgram=copy(test ? config->test.program : "");
    task->source=xrtValueClone(project->source); task->log.bytes=malloc(RING_INITIAL); task->terminal.bytes=malloc(RING_INITIAL); task->log.capacity=task->terminal.capacity=RING_INITIAL;
    if(!task->projectId || !task->configurationId || !task->name || !task->manifest || !task->kind || !task->entry || !task->compiler || !task->directory || !task->workingDirectory || !task->output || !task->source || !task->log.bytes || !task->terminal.bytes) goto allocationFailure;
    if(!task->testProgram) goto allocationFailure;
    char* outputDirectory=test ? copy(task->directory) : xidePathDirectory(task->output); GUID guid;
    if(!outputDirectory || CoCreateGuid(&guid)!=S_OK) { free(outputDirectory); goto allocationFailure; }
    char relative[160]; snprintf(relative,sizeof(relative),".xlang-ide-build/%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",(unsigned long)guid.Data1,guid.Data2,guid.Data3,guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    task->stageDirectory=xidePathResolve(outputDirectory,relative); free(outputDirectory);
    if(!task->stageDirectory) goto allocationFailure;
    char* cacheRoot=xidePathDirectory(task->stageDirectory); if(cacheRoot) task->maintainCache=claimMaintenance(tasks,cacheRoot); free(cacheRoot);
    task->artifact=xidePathResolve(task->stageDirectory,!strcmp(task->kind,"exe") ? "program.exe" : "module.dll"); task->snapshot=xidePathResolve(task->stageDirectory,"request.xlp");
    task->probeLog=xidePathResolve(task->stageDirectory,"compiler-help.log"); task->buildLog=xidePathResolve(task->stageDirectory,"build.log"); task->runLog=xidePathResolve(task->stageDirectory,"run.log"); task->promotion=xidePathResolve(task->stageDirectory,"publish.tmp");
    task->testLog=xidePathResolve(task->stageDirectory,"test.log");
    task->receipt=xidePathResolve(task->stageDirectory,"inputs.json"); task->invocation=xidePathResolve(task->stageDirectory,"build-invocation.json");
    if(!task->artifact || !task->snapshot || !task->probeLog || !task->buildLog || !task->runLog || !task->promotion || !task->testLog) goto allocationFailure;
    if(!task->receipt || !task->invocation) goto allocationFailure;
    if(!stringsCopy(&task->cSources,&project->cSources) || !stringsCopy(&task->resources,&project->resources) || !stringsCopy(&task->includePaths,&config->includePaths) || !stringsCopy(&task->libraryPaths,&config->libraryPaths) || !stringsCopy(&task->linkLibraries,&config->linkLibraries) || !stringsCopy(&task->arguments,test ? &config->test.arguments : &config->arguments)) goto allocationFailure;
    if(pendingInputs && pendingInputs->count && (!stringsCopy(&task->changedPaths,pendingInputs) || !stringsCopy(&task->pendingInputs,pendingInputs))) goto allocationFailure;
    if(!addInput(task,project->manifest,1) || !addInput(task,project->entry,!test)) goto allocationFailure;
    for(size_t i=0;i<project->cSources.count;++i) if(!addInput(task,project->cSources.items[i],!test)) goto allocationFailure;
    for(size_t i=0;i<project->resources.count;++i) if(!addInput(task,project->resources.items[i],!test)) goto allocationFailure;
    for(size_t i=0;i<project->files.count;++i) if(!addInput(task,project->files.items[i].path,0)) goto allocationFailure;
    for(size_t i=0;i<task->inputCount;++i) if(!strcmp(task->inputs[i].path,task->output)) { fail(error,"Output would overwrite a project input"); destroyTask(task); return NULL; }
    if(!test && !strcmp(task->compiler,task->output)) { fail(error,"Output would overwrite the compiler"); destroyTask(task); return NULL; }
    xvalue* environment=test ? config->test.environment : config->environment;
    size_t environmentCount=xrtValueCount(environment); task->environment=calloc(environmentCount+1,sizeof(*task->environment)); if(!task->environment) goto allocationFailure; task->environmentCount=environmentCount;
    for(size_t i=0;i<task->environmentCount;++i) {
        xstrview key,value; xvalue* item=xrtValueObjectAt(environment,i,&key);
        if(!xrtValueGetString(item,&value)) goto allocationFailure;
        char* name=malloc(key.Size+1); char* text=malloc(value.Size+1);
        task->environment[i]=(xprocessenv){name,text}; if(!name || !text) goto allocationFailure;
        memcpy(name,key.Data,key.Size); name[key.Size]=0; memcpy(text,value.Data,value.Size); text[value.Size]=0;
    }
    if(tasks->count==tasks->capacity) {
        size_t capacity=tasks->capacity ? tasks->capacity*2 : 16; xide_task** items=realloc(tasks->items,capacity*sizeof(*items)); if(!items) goto allocationFailure;
        tasks->items=items; tasks->capacity=capacity;
    }
    tasks->items[tasks->count++]=task; xideTasksPoll(tasks); return task;
allocationFailure:
    fail(error,"Unable to allocate the immutable task request"); destroyTask(task); return NULL;
}
xide_task* xideTasksBuild(xide_tasks* tasks,const xide_project* project,const char* configurationId,const char* compiler,int run,xide_model_error* error)
{ return startTask(tasks,project,configurationId,compiler,run,0,NULL,error); }
xide_task* xideTasksBuildGuarded(xide_tasks* tasks,const xide_project* project,const char* configurationId,const char* compiler,int run,const xide_strings* pendingInputs,xide_model_error* error)
{ return startTask(tasks,project,configurationId,compiler,run,0,pendingInputs,error); }
xide_task* xideTasksTest(xide_tasks* tasks,const xide_project* project,const char* configurationId,xide_model_error* error)
{ return startTask(tasks,project,configurationId,NULL,0,1,NULL,error); }
void xideTasksPoll(xide_tasks* tasks)
{
    if(!tasks) return;
    for(size_t i=0;i<tasks->count;++i) retireRequest(tasks->items[i]);
    if(tasks->active) { xide_task_info info={0}; xideTaskInfo(tasks->active,&info); if(info.state<=XIDE_TASK_CANCELLING) return; tasks->active=NULL; }
    for(size_t i=0;i<tasks->count;++i) {
        xide_task* task=tasks->items[i]; xide_task_info info={0}; xideTaskInfo(task,&info);
        if(info.state!=XIDE_TASK_QUEUED) continue;
        EnterCriticalSection(&task->lock);
        if(task->cancel) { task->info.state=XIDE_TASK_CANCELLED; task->info.phase=XIDE_TASK_DONE; ++task->info.revision; LeaveCriticalSection(&task->lock); continue; }
        task->began=GetTickCount64(); task->info.state=XIDE_TASK_EXECUTING; ++task->info.revision; LeaveCriticalSection(&task->lock);
        task->worker=CreateThread(NULL,0,taskWorker,task,0,NULL);
        if(!task->worker) { setFailure(task,"Unable to create build worker"); EnterCriticalSection(&task->lock); task->info.state=XIDE_TASK_FAILED; task->info.phase=XIDE_TASK_DONE; LeaveCriticalSection(&task->lock); continue; }
        tasks->active=task; return;
    }
}
int xideTasksBusy(const xide_tasks* tasks)
{
    if(!tasks) return 0;
    for(size_t i=0;i<tasks->count;++i) { xide_task_info info={0}; xideTaskInfo(tasks->items[i],&info); if(info.state<=XIDE_TASK_CANCELLING) return 1; } return 0;
}
size_t xideTasksCount(const xide_tasks* tasks) { return tasks ? tasks->count : 0; }
xide_task* xideTasksAt(xide_tasks* tasks,size_t index) { return tasks && index<tasks->count ? tasks->items[index] : NULL; }
int xideTasksContains(const xide_tasks* tasks,const xide_task* task)
{
    if(tasks && task) for(size_t i=0;i<tasks->count;++i) if(tasks->items[i]==task) return 1;
    return 0;
}
size_t xideTasksTrimHistory(xide_tasks* tasks,size_t keepCompleted,const xide_task* const* protectedTasks,size_t protectedCount)
{
    if(!tasks || (protectedCount && !protectedTasks)) return 0;
    size_t eligible=0,removed=0;
    for(size_t i=0;i<tasks->count;++i) {
        xide_task* task=tasks->items[i]; retireRequest(task); int protected=0;
        for(size_t j=0;j<protectedCount;++j) if(protectedTasks[j]==task) { protected=1; break; }
        if(!protected && completedWorker(task)) ++eligible;
    }
    size_t drop=eligible>keepCompleted ? eligible-keepCompleted : 0,write=0;
    for(size_t i=0;i<tasks->count;++i) {
        xide_task* task=tasks->items[i]; int protected=0;
        for(size_t j=0;j<protectedCount;++j) if(protectedTasks[j]==task) { protected=1; break; }
        if(drop && !protected && completedWorker(task)) { destroyTask(task); --drop; ++removed; }
        else tasks->items[write++]=task;
    }
    tasks->count=write; return removed;
}
void xideTaskInfo(xide_task* task,xide_task_info* info)
{
    if(!task || !info) return;
    EnterCriticalSection(&task->lock); *info=task->info; info->invalidated=task->invalidated; if(task->began && info->state<=XIDE_TASK_CANCELLING) info->durationMs=GetTickCount64()-task->began; if(task->cancel && info->state==XIDE_TASK_EXECUTING) info->state=XIDE_TASK_CANCELLING; LeaveCriticalSection(&task->lock);
}
const char* xideTaskProjectId(const xide_task* task) { return task ? task->projectId : ""; }
const char* xideTaskConfigurationId(const xide_task* task) { return task ? task->configurationId : ""; }
const char* xideTaskProjectName(const xide_task* task) { return task ? task->name : ""; }
const char* xideTaskArtifact(const xide_task* task) { return task && !task->test ? task->artifact : ""; }
const char* xideTaskOutput(const xide_task* task) { return task ? task->output : ""; }
const char* xideTaskBuildLog(const xide_task* task) { return task && !task->test ? task->buildLog : ""; }
const char* xideTaskRunLog(const xide_task* task) { return task && !task->test ? task->runLog : ""; }
const char* xideTaskTestLog(const xide_task* task) { return task && task->test ? task->testLog : ""; }
const char* xideTaskInputReceipt(const xide_task* task) { return task && !task->test ? task->receipt : ""; }
char* xideTaskInputPath(xide_task* task,size_t index)
{
    if(!task) return NULL;
    EnterCriticalSection(&task->lock); char* path=index<task->inputCount ? copy(task->inputs[index].path) : NULL; LeaveCriticalSection(&task->lock); return path;
}
size_t xideTaskInputCount(xide_task* task)
{
    if(!task) return 0;
    EnterCriticalSection(&task->lock); size_t count=task->inputCount; LeaveCriticalSection(&task->lock); return count;
}
static size_t readRing(xide_task* task,ring* source,uint64_t* cursor,void* bytes,size_t capacity,uint64_t* dropped)
{
    if(dropped) *dropped=0;
    if(!task || !cursor || !bytes) return 0;
    EnterCriticalSection(&task->lock);
    if(*cursor<source->base) { if(dropped) *dropped=source->base-*cursor; *cursor=source->base; }
    uint64_t offset=*cursor-source->base; size_t size=offset<source->count ? source->count-(size_t)offset : 0; if(size>capacity) size=capacity;
    size_t start=(source->start+(size_t)offset)%source->capacity,first=source->capacity-start; if(first>size) first=size;
    memcpy(bytes,source->bytes+start,first); memcpy((unsigned char*)bytes+first,source->bytes,size-first); *cursor+=size;
    LeaveCriticalSection(&task->lock); return size;
}
size_t xideTaskReadLog(xide_task* task,uint64_t* cursor,void* bytes,size_t capacity,uint64_t* dropped) { return task ? readRing(task,&task->log,cursor,bytes,capacity,dropped) : 0; }
size_t xideTaskReadTerminal(xide_task* task,uint64_t* cursor,void* bytes,size_t capacity,uint64_t* dropped) { return task ? readRing(task,&task->terminal,cursor,bytes,capacity,dropped) : 0; }
int xideTaskWrite(xide_task* task,const void* bytes,size_t size)
{
    if(!task) return 0;
    EnterCriticalSection(&task->lock); int okay=(task->info.phase==XIDE_TASK_RUN || task->info.phase==XIDE_TASK_TEST) && task->process && xideProcessWrite(task->process,bytes,size); LeaveCriticalSection(&task->lock); return okay;
}
int xideTaskInputOpen(xide_task* task)
{
    if(!task) return 0;
    EnterCriticalSection(&task->lock);
    int open=task->info.state==XIDE_TASK_EXECUTING && (task->info.phase==XIDE_TASK_RUN || task->info.phase==XIDE_TASK_TEST) && xideProcessInputOpen(task->process);
    LeaveCriticalSection(&task->lock); return open;
}
void xideTaskCloseInput(xide_task* task)
{
    if(task) { EnterCriticalSection(&task->lock); if((task->info.phase==XIDE_TASK_RUN || task->info.phase==XIDE_TASK_TEST) && task->process) xideProcessCloseInput(task->process); LeaveCriticalSection(&task->lock); }
}
void xideTaskCancel(xide_task* task)
{
    if(task) { EnterCriticalSection(&task->lock); if(task->info.state<=XIDE_TASK_CANCELLING) { task->cancel=1; if(task->info.state==XIDE_TASK_QUEUED) { task->info.state=XIDE_TASK_CANCELLED; task->info.phase=XIDE_TASK_DONE; } if(task->process) xideProcessCancel(task->process); ++task->info.revision; } LeaveCriticalSection(&task->lock); }
}
void xideTasksInvalidateInput(xide_tasks* tasks,const char* path)
{
    if(!tasks || !path) return;
    for(size_t i=0;i<tasks->count;++i) {
        xide_task* task=tasks->items[i]; xide_task_info info={0}; xideTaskInfo(task,&info);
        if(info.state>XIDE_TASK_CANCELLING || (info.phase==XIDE_TASK_RUN || info.phase==XIDE_TASK_TEST) || info.invalidated) continue;
        EnterCriticalSection(&task->lock);
        int matches=0;
        for(size_t j=0;j<task->inputCount;++j) if(!strcmp(task->inputs[j].path,path)) { matches=1; break; }
        if(task->info.state<=XIDE_TASK_CANCELLING && task->info.phase!=XIDE_TASK_RUN && task->info.phase!=XIDE_TASK_TEST) {
            if(matches) { task->invalidated=1; if(task->process) xideProcessCancel(task->process); ++task->info.revision; }
            else {
                /* A hard-link alias can have a different canonical spelling.
                   Queue its identity check without filesystem work in the UI. */
                int duplicate=0; for(size_t j=0;j<task->changedPaths.count;++j) if(!strcmp(task->changedPaths.items[j],path)) { duplicate=1; break; }
                if(!duplicate && task->changedPaths.count>=8192) task->invalidated=1;
                else if(!duplicate) {
                    if(task->changedPaths.count==task->changedPaths.capacity) {
                        size_t capacity=task->changedPaths.capacity ? task->changedPaths.capacity*2 : 8; char** items=realloc(task->changedPaths.items,capacity*sizeof(*items));
                        if(items) { task->changedPaths.items=items; task->changedPaths.capacity=capacity; }
                    }
                    char* value=task->changedPaths.count<task->changedPaths.capacity ? copy(path) : NULL;
                    if(value) task->changedPaths.items[task->changedPaths.count++]=value;
                    else task->invalidated=1; /* Memory failure never allows a stale run. */
                }
            }
        }
        LeaveCriticalSection(&task->lock);
    }
}
size_t xideTaskDiagnosticCount(xide_task* task)
{
    if(!task) return 0;
    EnterCriticalSection(&task->lock); size_t count=task->diagnosticCount; LeaveCriticalSection(&task->lock); return count;
}
int xideTaskDiagnostic(xide_task* task,size_t index,xide_diagnostic* diagnostic)
{
    if(!task || !diagnostic) return 0;
    memset(diagnostic,0,sizeof(*diagnostic)); EnterCriticalSection(&task->lock);
    if(index>=task->diagnosticCount) { LeaveCriticalSection(&task->lock); return 0; }
    *diagnostic=task->diagnostics[index]; diagnostic->path=copy(diagnostic->path); diagnostic->message=copy(diagnostic->message); LeaveCriticalSection(&task->lock);
    if(!diagnostic->path || !diagnostic->message) { xideDiagnosticClear(diagnostic); return 0; } return 1;
}
void xideDiagnosticClear(xide_diagnostic* diagnostic)
{
    if(diagnostic) { free(diagnostic->path); free(diagnostic->message); memset(diagnostic,0,sizeof(*diagnostic)); }
}

xide_toolchains* xideTasksToolchains(xide_tasks* tasks) { return tasks ? tasks->toolchains : NULL; }
int xideTaskToolchain(xide_task* task,xide_toolchain_profile* out) { if(!task || !out) return 0; EnterCriticalSection(&task->lock); int okay=task->toolchain.help && xideToolchainProfileCopy(out,&task->toolchain); LeaveCriticalSection(&task->lock); return okay; }
