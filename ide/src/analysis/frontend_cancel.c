#include "analysis/frontend_cancel.h"
#include <string.h>
_Thread_local xls_frontend_cancel_scope* xlsActiveFrontendCancel;
void xlsFrontendCancelBegin(xls_frontend_cancel_scope* scope,int (*cancelled)(void*),void* user)
{
    if(!scope)return;
    memset(scope,0,sizeof(*scope));scope->previous=xlsActiveFrontendCancel;
    scope->cancelled=cancelled;scope->user=user;
    if(cancelled || scope->previous){
        scope->active=1;scope->work.cancelled=xlsFrontendCancelObserved();
        xlsActiveFrontendCancel=scope;
    }
}
void xlsFrontendCancelEnd(xls_frontend_cancel_scope* scope,xls_frontend_cancel_work* work)
{
    if(work)*work=(xls_frontend_cancel_work){0};
    if(!scope || (scope->active && xlsActiveFrontendCancel!=scope))return;
    if(work)*work=scope->work;
    if(scope->active){xlsActiveFrontendCancel=scope->previous;scope->active=0;}
}
int xlsFrontendCancelPoll(void)
{
    xls_frontend_cancel_scope* current=xlsActiveFrontendCancel;
    for(xls_frontend_cancel_scope* scope=current;scope;scope=scope->previous){
        int stop=scope->work.cancelled;
        if(!stop && scope->cancelled && !scope->polling){
            ++scope->work.predicateCalls;scope->polling=1;
            stop=scope->cancelled(scope->user);scope->polling=0;
        }
        if(stop){scope->work.cancelled=1;current->work.cancelled=1;return 1;}
    }
    return 0;
}
xls_frontend_cancel_work xlsFrontendCancelWork(void)
{return xlsActiveFrontendCancel ? xlsActiveFrontendCancel->work : (xls_frontend_cancel_work){0};}
