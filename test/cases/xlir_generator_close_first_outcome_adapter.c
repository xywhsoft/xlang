/* Include actual generated C in this TU. The surviving runtime keeps all
 * 900 source-call and 2700 repeated-close scenarios and its allocator checks. */
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#error "Pass current-source generator C"
#endif
#ifndef XLIR_GENERATOR_STORAGE_BINDING
#error "Pass its verified START binding"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
#include XLIR_GENERATOR_STORAGE_BINDING
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned marks[3];static bool normalAdvance;
void close_outcome_mark(int64_t id)
{
    assert(id==10 || id==11 || (id==90 && normalAdvance));
    ++marks[id==90?2:id==11];
}
void close_outcome_fail(int64_t mode,int64_t code)
{
    assert(mode>=0 && mode<=2 && (code==31 || code==42));
    if(!mode)return;
    xerror* error=mode==1?xrtErrorCreate(XERR_STATE,"generator-outcome",(int)code,"expected first outcome"):NULL;
    assert(mode==2 || error);xl6ErrorScopePropagateFailure((xl6_error_result){false,error});
}
static void check(xl6_error_result outcome,int mode,int code)
{
    assert(outcome.ok==(mode==0));
    if(mode==1)assert(outcome.error && xrtErrorKind(outcome.error)==XERR_STATE &&
        xrtErrorCode(outcome.error)==code && !strcmp(xrtErrorDomain(outcome.error),"generator-outcome"));
    else assert(!outcome.error);
}
void close_outcome_prepare(void){assert(xlir_module_init() && !xrtGetError());}
void close_outcome_finish(void){xlir_module_fini();assert(!xrtGetError());}
int close_outcome_failures(void){return 0;}
void close_outcome_case(int consumer,int cleanup)
{
    memset(marks,0,sizeof(marks));xl6_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
    int64_t value=close_outcome_consume(consumer,cleanup);xl6_error_result outcome=xl6ForeignCallEnd(&scope);
    int mode=consumer?consumer:cleanup;check(outcome,mode,consumer?31:42);
    assert(value==(mode?0:7) && marks[0]==1 && marks[1]==1 && !marks[2] && !xrtGetError());
    xrtErrorFree(outcome.error);
}
void close_outcome_repeated(int state,int consumer,int cleanup)
{
    assert(state>=0 && state<=2);memset(marks,0,sizeof(marks));
    xlir_result_generator launched=CLOSE_OUTCOME_START(cleanup);assert(launched.ok && launched.value && !xrtGetError());
    xlir_generator* generator=launched.value;
    normalAdvance=true;
    for(int i=0;i<state;++i){xlir_generator_step step=xlir_generator_next(generator);assert(step.ok && step.value && !xrtGetError());}
    normalAdvance=false;assert(marks[2]==(unsigned)(state==2));
    xl6_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
    close_outcome_fail(consumer,31);
    int mode=consumer?consumer:state?cleanup:0,code=consumer?31:42;
    for(unsigned i=0;i<3;++i){
        xl6_error_result closed=xlir_generator_close(generator);check(closed,mode,code);
        assert(marks[0]==(unsigned)(state!=0) && marks[1]==(unsigned)(state!=0));
        /* Close returns its own result; explicitly publish it at this native
         * boundary, preserving a NULL first diagnostic as a real failure. */
        if(!closed.ok)xl6ErrorScopePropagateFailure(closed);else xrtErrorFree(closed.error);
    }
    xlir_generator_destroy(generator);
    xl6_error_result outcome=xl6ForeignCallEnd(&scope);check(outcome,mode,code);
    assert(!xrtGetError());xrtErrorFree(outcome.error);
}
