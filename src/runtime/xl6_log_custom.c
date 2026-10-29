#include "src/runtime/xl6_log_record.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

typedef struct log_custom {
    xvalue* write;
    xvalue* flush;
    void* sink;
    uint64_t thread;
    unsigned active;
    bool closed;
} log_custom;
static bool callback(xvalue* callable,xvalue* argument,int64_t* value,bool boolean)
{
    xrtcallframe frame={0};
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    xl6_foreign_error_scope invocation={0};
    bool flag=false, ok;
    xvalue* arguments[1]={argument};
    frame.ArgumentCount=argument ? 1:0; frame.Arguments=argument ? arguments:NULL;
    if (!xl6ForeignCallBegin(&invocation)) return false;
    ok=xrtValueInvoke(callable,&frame,&result);
    if (ok && xrtCallResultCount(&result)!=1) { xrtSetErrorKind(XERR_TYPE); ok=false; }
    if (ok) {
        xvalue* returned=xrtCallResultGet(&result,0);
        ok=boolean ? xrtValueGetBool(returned,&flag) : xrtValueGetInt(returned,value);
        if (boolean) *value=flag ? 1:0;
    }
    xrtCallResultUnit(&result);
    if (!ok && !invocation.failed) xl6ErrorScopeCaptureFailure(&invocation);
    xl6_foreign_error_result outcome=xl6ForeignCallEnd(&invocation);
    ok=ok && outcome.ok;
    xrtSetErrorTake(outcome.error);
    if (!ok) xl6ForeignCallbackFail();
    return ok;
}
static bool custom_thread(log_custom* state)
{
    if (state->thread==xrtThreadCurrentId()) return true;
    xrtSetErrorKind(XERR_STATE); return false;
}
static xlogresult custom_write(const xlogrecord* record,void* context)
{
    log_custom* state=context;
    xl6_log_record borrowed={*record,(xlogfield*)record->Fields,record->FieldCount};
    xvalue* argument;
    int64_t value;
    bool ok;
    if (!custom_thread(state)) return XLOG_RESULT_ERROR;
    if (state->closed) { xrtSetErrorKind(XERR_CLOSED); return XLOG_RESULT_ERROR; }
    argument=xrtValuePointer(&borrowed);
    if (!argument) return XLOG_RESULT_ERROR;
    ++state->active;
    ok=callback(state->write,argument,&value,false);
    --state->active;
    xrtValueRelease(argument);
    if (!ok) return XLOG_RESULT_ERROR;
    if (value<XLOG_RESULT_SKIPPED || value>XLOG_RESULT_DROPPED) { xrtSetErrorKind(XERR_RANGE); return XLOG_RESULT_ERROR; }
    return (xlogresult)value;
}
static bool custom_flush(void* context)
{
    log_custom* state=context; int64_t value;
    if (!custom_thread(state)) return false;
    if (state->closed) { xrtSetErrorKind(XERR_CLOSED); return false; }
    if (!state->flush || xrtValueType(state->flush)==XVALUE_NULL) return true;
    ++state->active;
    bool ok=callback(state->flush,NULL,&value,true) && value!=0;
    --state->active;
    return ok;
}
static void custom_drop(void* context)
{
    log_custom* state=context;
    xerror* prior=xrtTakeError();
    if (state->sink) (void)xl6LogSinkAffinityV1(state->sink,false,NULL);
    xrtValueRelease(state->write); xrtValueRelease(state->flush); xrtFree(state);
    xrtSetErrorTake(prior);
}
void* xl6LogCustomV1(xstrview name,int64_t level,xvalue* write,xvalue* flush)
{
    log_custom* state;
    xlogsinkconfig config;
    if (level<0 || level>6) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    if (!write) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xrtValueGetCallable(write) ||
        (flush && xrtValueType(flush)!=XVALUE_NULL && !xrtValueGetCallable(flush))) return NULL;
    state=xrtMalloc(sizeof(*state)); if (!state) return NULL;
    memset(state,0,sizeof(*state));
    state->write=xrtValueRetain(write); state->flush=flush ? xrtValueRetain(flush):NULL;
    if (!state->write || (flush && !state->flush)) { custom_drop(state); return NULL; }
    state->thread=xrtThreadCurrentId();
    memset(&config,0,sizeof(config)); config.Name=name; config.Level=(xloglevel)level;
    config.Write=custom_write; config.Flush=custom_flush; config.Drop=custom_drop; config.UserData=state;
    state->sink=xrtLogSinkCreate(&config);
    if (!state->sink) { custom_drop(state); return NULL; }
    if (!xl6LogSinkAffinityV1(state->sink,true,state)) {
        xrtLogSinkFree(state->sink); return NULL;
    }
    return state->sink;
}
bool xl6LogCustomCloseV1(void* sink)
{
    log_custom* state=xl6LogSinkAffinityContextV1(sink);
    if (!state || !custom_thread(state)) return false;
    if (state->closed) return true;
    if (state->active) { xrtSetErrorKind(XERR_STATE); return false; }
    bool ok=custom_flush(state);
    state->closed=true;
    xerror* prior=xrtTakeError();
    xvalue* write=state->write; xvalue* flush=state->flush;
    state->write=NULL; state->flush=NULL;
    xrtValueRelease(write); xrtValueRelease(flush);
    xrtSetErrorTake(prior);
    return ok;
}
