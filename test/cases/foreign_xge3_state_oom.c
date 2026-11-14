#include "foreign_xge3_state_oom.h"
#include <assert.h>
void fixtureStateOomBudget(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else{
        assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
        assert(xrtMemDebugFailAfter(allowance));
    }
}
