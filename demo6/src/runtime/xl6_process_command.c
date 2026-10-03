#include "src/runtime/xl6_process_config.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6ProcessCommandDestroyV1(void* handle)
{
    XL6_ProcessCommand* command=handle;if(!command)return;
    xerror* prior=xrtTakeError();xl6ProcessConfigUnit(&command->config);
    for(unsigned stream=0;stream<3;++stream)xl6ProcessIoLeaseDrop(&command->stdio[stream]);
    xl6TextThreadDrop(&command->owner);xrtFree(command);
    xrtClearError();xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessCommandHandlePolicyV1Get, xl6ProcessCommandDestroyV1)
static XL6_ProcessCommand* commandCreate(void)
{
    XL6_ProcessCommand* command=xrtCalloc(1,sizeof(*command));if(!command)return NULL;
    if(!xl6TextThreadInit(&command->owner) || !xrtProcessConfigInit(&command->config)) {
        xl6ProcessCommandDestroyV1(command);return NULL;
    }
    command->config.HideWindow=true;
    command->config.Stdin.Mode=XPROCESS_IO_PIPE;
    command->config.Stdout.Mode=XPROCESS_IO_PIPE;
    command->config.Stderr.Mode=XPROCESS_IO_PIPE;
    return command;
}
void* xl6ProcessCommandDirectV1(xstrview program,const xvalue* args,int64_t size,int64_t argSize)
{
    if(!args || xrtValueType(args)!=XVALUE_ARRAY){xrtSetErrorKind(XERR_TYPE);return NULL;}
    if(argSize < -1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    size_t count=xrtValueCount(args);
    if(count>SIZE_MAX/sizeof(cstr)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_ProcessCommand* command=commandCreate();if(!command)return NULL;
    command->config.Program=xl6ProcessCString(program,size,true);if(!command->config.Program)goto fail;
    cstr* argv=count ? xrtCalloc(count,sizeof(*argv)) : NULL;
    if(count && !argv)goto fail;
    command->config.Args=argv;
    for(size_t i=0;i<count;++i) {
        xstrview item;
        if(!xrtValueGetString(xrtValueArrayGet(args,i),&item))goto fail;
        argv[i]=xl6ProcessCString(item,argSize,false);if(!argv[i])goto fail;
        ++command->config.ArgCount;
    }
    return command;
fail:xl6ProcessCommandDestroyV1(command);return NULL;
}
void* xl6ProcessCommandShellV1(xstrview text,int64_t size)
{
    XL6_ProcessCommand* command=commandCreate();if(!command)return NULL;
    command->config.Command=xl6ProcessCString(text,size,true);
    if(!command->config.Command){xl6ProcessCommandDestroyV1(command);return NULL;}
    command->config.Target=XPROCESS_SHELL;return command;
}
static void commandText(void* handle,xstrview value,int64_t size,bool arg0)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    char* copy=xl6ProcessCString(value,size,false);if(!copy)return;
    cstr* field=arg0 ? &command->config.Arg0 : &command->config.WorkDir;
    xrtFree((void*)*field);*field=copy;
}
void xl6ProcessCommandWorkDirV1(void* h,xstrview v,int64_t size){commandText(h,v,size,false);}
void xl6ProcessCommandArg0V1(void* h,xstrview v,int64_t size){commandText(h,v,size,true);}
static void commandEnv(void* handle,xstrview name,xstrview value,int64_t nameSize,int64_t valueSize,bool remove)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    char* key=xl6ProcessCString(name,nameSize,true);if(!key)return;
    if(strchr(key,'=')){xrtFree(key);xrtSetErrorKind(XERR_VALUE);return;}
    char* text=remove ? NULL : xl6ProcessCString(value,valueSize,false);
    if(!remove && !text){xrtFree(key);return;}
    xprocessconfig* c=&command->config;size_t index=0;
    while(index<c->EnvCount && strcmp(c->Env[index].Name,key))++index;
    if(index==c->EnvCount && index==command->envCapacity) {
        size_t capacity=command->envCapacity ? command->envCapacity*2 : 8;
        if(capacity<command->envCapacity || capacity>SIZE_MAX/sizeof(xprocessenv)) {
            xrtSetErrorKind(XERR_RANGE);goto fail;
        }
        xprocessenv* grown=xrtRealloc((void*)c->Env,capacity*sizeof(*grown));if(!grown)goto fail;
        c->Env=grown;command->envCapacity=capacity;
    }
    xprocessenv* env=(xprocessenv*)c->Env;
    if(index<c->EnvCount){xrtFree((void*)env[index].Name);xrtFree((void*)env[index].Value);}
    else ++c->EnvCount;
    env[index]=(xprocessenv){key,text};return;
fail:xrtFree(key);xrtFree(text);
}
void xl6ProcessCommandEnvV1(void* h,xstrview n,xstrview v,int64_t ns,int64_t vs)
{commandEnv(h,n,v,ns,vs,false);}
void xl6ProcessCommandRemoveEnvV1(void* h,xstrview n,int64_t size)
{commandEnv(h,n,(xstrview){"",0},size,-1,true);}
void xl6ProcessCommandClearEnvV1(void* handle)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    xprocessconfig* c=&command->config;
    for(size_t i=0;i<c->EnvCount;++i){xrtFree((void*)c->Env[i].Name);xrtFree((void*)c->Env[i].Value);}
    c->EnvCount=0;
}
#define FLAG(Name,Field) \
void xl6ProcessCommand##Name##V1(void* handle,bool enabled) { \
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(command)command->config.Field=enabled; }
FLAG(InheritEnv,InheritEnv)
FLAG(NewConsole,NewConsole)
FLAG(ProcessGroup,NewGroup)
FLAG(HideWindow,HideWindow)
#undef FLAG
void xl6ProcessCommandStdioV1(void* handle,int64_t stream,int64_t mode)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    /* Raw HANDLE is never a language integer. File bindings carry an owned
     * native lease; generic VFS streams use explicit pipe IO instead. */
    if(stream<XPROCESS_STDIN || stream>XPROCESS_STDERR || mode<XPROCESS_IO_INHERIT ||
       mode>XPROCESS_IO_MERGE || mode==XPROCESS_IO_HANDLE || (mode==XPROCESS_IO_MERGE && stream!=XPROCESS_STDERR)) {
        xrtSetErrorKind(XERR_RANGE);return;
    }
    xprocessio* io=stream==XPROCESS_STDIN ? &command->config.Stdin :
        stream==XPROCESS_STDOUT ? &command->config.Stdout : &command->config.Stderr;
    io->Mode=(xprocessiomode)mode;
    io->Handle=-1;xl6ProcessIoLeaseDrop(&command->stdio[stream]);
}
