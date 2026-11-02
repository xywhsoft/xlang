#ifndef XLS_LIFECYCLE_PROFILE_H
    #define XLS_LIFECYCLE_PROFILE_H
    #include <xrt.h>
void xlsLifecycleBegin(void);void xlsLifecycleAdd(size_t,double);void xlsLifecycleStep(size_t,double*);void xlsLifecycleEmit(const char*);
    #define XLS_LIFECYCLE(id,expression) ({double lifecycleStart=xrtTimer();__auto_type lifecycleResult=(expression);xlsLifecycleAdd(id,lifecycleStart);lifecycleResult;})
    #define XLS_LIFECYCLE_VOID(id,expression) do{double lifecycleStart=xrtTimer();expression;xlsLifecycleAdd(id,lifecycleStart);}while(0)
    #endif
    