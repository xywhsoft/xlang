#define XLIR_ESCAPE_NO_MAIN
#include "xlir_object_escape_tcc_host.c"

static unsigned failure_mode, injected;
static void failing_drop(int64_t id)
{
    drop_escape(id);
    if (injected++) return; /* One failed physical duty; later duties still run. */
    if (!(failure_mode & 1)) {
        xerror* cause=xrtErrorCreate(XERR_RANGE,"retirement",177,"original cause");
        xerror* error=xrtErrorWrap(cause,XERR_STATE,"retirement",178,"destructor failed");
        assert(cause && error); xrtErrorFree(cause); xrtSetErrorTake(error);
    } else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
int main(int argc,char** argv)
{
    assert(argc==2);failure_mode=(unsigned)strtoul(argv[1],NULL,10);assert(failure_mode<4);
    expected_cleanups=0;assert(xrtMemDebugEnable(true));provider_init();
    for(unsigned round=0;round<25;++round){
        typedef void (*Bind)(const EscapeHooks*);Bind bind;
        EscapeHooks hooks={capture_escape,failing_drop};xmemdebugsnapshot before,after;
        xl6_error_scope scope={0};xl6_error_result outcome;xerror* prior=NULL;
        memset(&context,0,sizeof(context));escaped=NULL;selected=4;
        injected=external_drops=internal_drops=0;
        atomic_store(&deleted,0);atomic_store(&cleanups,0);xrtClearError();
        xrtMemDebugSnapshot(&before);load_artifact();
        {void* symbol=lookup(compiled_state,"escape_bind");memcpy(&bind,&symbol,sizeof(bind));}
        assert(bind);bind(&hooks);xl__tcc.tcc_get_symbol=lookup;xl__tcc.tcc_delete=delete_state;
        xlApiLock();module=xlCreateModuleFromState(&context,compiled_state);xlApiUnlock();assert(module);
        assert(xlActivateModule(&context,module,NULL) && !escape_external_owner());
        if(failure_mode&2){prior=xrtErrorCreate(XERR_IO,"host",179,"prior host error");assert(prior);}
        assert(xl6ErrorScopeBegin(&scope));xrtSetErrorTake(prior);
        xlDestroyModule(module);outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && !atomic_load(&deleted) && injected==1 && internal_drops==1);
        assert(module->retirementFailed && !module->ownershipPlan);
        if(failure_mode&1)assert(!module->retirementError);
        else assert(module->retirementError && xrtErrorCode(module->retirementError)==178 &&
            xrtErrorCode(xrtErrorCause(module->retirementError))==177);
        if(prior)assert(outcome.error==prior);
        else if(failure_mode&1)assert(!outcome.error);
        else assert(outcome.error==module->retirementError);
        /* An external error alias does NOT own executable code. Retry must
         * admit its immutable cause DAG, finish remaining semantic duties,
         * and unmap the actual TCC allocation without replaying the failure. */
        finish();assert(injected==3 && internal_drops==3 && !external_drops);
        if(outcome.error)assert(xrtErrorCode(outcome.error)==(prior?179:178));
        xrtErrorFree(outcome.error);assert(!xrtGetError());xrtFree(context.lastError);
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    }
    printf("class retirement outcome mode=%u: 25 real TCC lifetimes, 75 once-only finalizers, explicit failure and first diagnostic, retry and error alias survive unmap; balanced memory\n",failure_mode);
    return 0;
}
