#ifndef XL6_PROCESS_CONFIG_H
#define XL6_PROCESS_CONFIG_H
#include "src/runtime/xl6_process_internal.h"

/* Private owned OS configuration, never a view into a language object. */
static inline void xl6ProcessConfigUnit(xprocessconfig* config)
{
    xerror* prior=xrtTakeError();
    xprocessconfig old=*config;memset(config,0,sizeof(*config));
    xrtFree((void*)old.Program);xrtFree((void*)old.Command);
    xrtFree((void*)old.Arg0);xrtFree((void*)old.WorkDir);
    for(size_t i=0;i<old.ArgCount;++i)xrtFree((void*)old.Args[i]);
    xrtFree((void*)old.Args);
    for(size_t i=0;i<old.EnvCount;++i){xrtFree((void*)old.Env[i].Name);xrtFree((void*)old.Env[i].Value);}
    xrtFree((void*)old.Env);xrtClearError();xrtSetErrorTake(prior);
}
static inline bool xl6ProcessConfigClone(xprocessconfig* copy,const xprocessconfig* source)
{
    *copy=*source;
    copy->Program=NULL;copy->Command=NULL;copy->Arg0=NULL;copy->WorkDir=NULL;
    copy->Args=NULL;copy->ArgCount=0;copy->Env=NULL;copy->EnvCount=0;
    /* Language command text is already validated UTF-8/NUL-free OS text.
     * Do not reconstruct a language string length from these private C views. */
#define COPY_TEXT(Field) if(source->Field && !(copy->Field=xrtStrDup(source->Field)))goto fail
    COPY_TEXT(Program);COPY_TEXT(Command);COPY_TEXT(Arg0);COPY_TEXT(WorkDir);
#undef COPY_TEXT
    if(source->ArgCount>SIZE_MAX/sizeof(cstr) || source->EnvCount>SIZE_MAX/sizeof(xprocessenv)){
        xrtSetErrorKind(XERR_RANGE);goto fail;
    }
    if(source->ArgCount){
        cstr* args=xrtCalloc(source->ArgCount,sizeof(*args));if(!args)goto fail;
        copy->Args=args;copy->ArgCount=source->ArgCount;
        for(size_t i=0;i<source->ArgCount;++i){
            args[i]=xrtStrDup(source->Args[i]);if(!args[i])goto fail;
        }
    }
    if(source->EnvCount){
        xprocessenv* env=xrtCalloc(source->EnvCount,sizeof(*env));if(!env)goto fail;
        copy->Env=env;copy->EnvCount=source->EnvCount;
        for(size_t i=0;i<source->EnvCount;++i){
            env[i].Name=xrtStrDup(source->Env[i].Name);if(!env[i].Name)goto fail;
            if(source->Env[i].Value){env[i].Value=xrtStrDup(source->Env[i].Value);if(!env[i].Value)goto fail;}
        }
    }
    return true;
fail:xl6ProcessConfigUnit(copy);return false;
}
#endif
