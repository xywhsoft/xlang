#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#error "Pass actual verified generator C"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int trace[16];static size_t traceCount;
void generator_close_mark(int64_t id)
{assert(traceCount<sizeof(trace)/sizeof(*trace));trace[traceCount++]=(int)id;}
void generator_close_raise(int64_t mode)
{
    assert(mode>=0 && mode<=2);if(!mode)return;
    xerror* error=mode==1?xrtErrorCreate(XERR_STATE,"generator-close",77,"expected pending failure"):NULL;
    assert(mode==2 || error);xl6ErrorScopePropagateFailure((xl6_error_result){false,error});
}
static void expect(const int* values,size_t count)
{
    if(count!=traceCount || memcmp(values,trace,count*sizeof(*trace))){
        fprintf(stderr,"close trace expected:");for(size_t i=0;i<count;++i)fprintf(stderr," %d",values[i]);
        fprintf(stderr," actual:");for(size_t i=0;i<traceCount;++i)fprintf(stderr," %d",trace[i]);fputc('\n',stderr);
    }
    assert(count==traceCount && !memcmp(values,trace,count*sizeof(*trace)));
}
void generator_close_prepare(void){assert(xlir_module_init() && !xrtGetError());}
void generator_close_finish(void){xlir_module_fini();assert(!xrtGetError());}
int generator_close_cases_failed(void){return 0;}
void generator_close_case(int form,int mode,int limit)
{
    assert(form>=0 && form<4);traceCount=0;xl6_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
    int64_t value=form==0?generator_close_consume(mode,limit):form==1?generator_close_nested_consume(limit):
        form==2?generator_close_pending_consume(mode):generator_close_return_consume();
    xl6_error_result result=xl6ForeignCallEnd(&scope);
    bool failed=form==2 && mode!=0;assert(result.ok==!failed && !xrtGetError());
    if(failed){
        assert(value==0);
        if(mode==1)assert(result.error && xrtErrorKind(result.error)==XERR_STATE &&
            xrtErrorCode(result.error)==77 && !strcmp(xrtErrorDomain(result.error),"generator-close"));
        else assert(!result.error);
    }else assert(!result.error);
    if(form==0){
        int expected[8];size_t n=0;expected[n++]=10;
        if(limit>1)expected[n++]=11;
        expected[n++]=20;if(mode)expected[n++]=21;expected[n++]=22;
        if(limit>(mode?2:1))expected[n++]=30;
        expect(expected,n);assert(value==(limit<(mode?3:2)?limit:(mode?3:2)));
    }else if(form==1){
        const int expected[]={20,40,41,21,30};expect(expected,limit>=4?5:4);assert(value==(limit<4?limit:4));
    }else if(form==2){
        const int expected[]={20,21};expect(expected,2);if(!failed)assert(value==7);
    }else{
        const int expected[]={20,21};expect(expected,2);assert(value==9);
    }
    xrtErrorFree(result.error);
}
