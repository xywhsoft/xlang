/* Local CPU backend. Every object is explicitly closed before main returns,
 * so this fixture does not claim qualification of unloaded provider code. */
#include "test/cases/foreign_xui_icon_models.h"
#include <assert.h>
#include "tests/xui/xui_test_proxy.c"
static xui_test_proxy_state_t backend;
static size_t references=1;
static bool hold(const void* raw){assert(raw==&references);++references;return true;}
static bool drop(const void* raw){assert(raw==&references && references>1);--references;return true;}
static bool count(const void* raw,size_t* output){assert(raw==&references);*output=references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{assert(raw==&references);(void)visit;(void)user;return true;}
static const xrtownershipops operations={count,trace};
int fixtureIconProxy(xui_proxy* output)
{
    xuiTestProxyInit(&backend);
    const xui_callback_owner_t owner={{&references,&operations},hold,drop};
    return xuiProxyCreateOwned(&backend.tProxy,&owner,output);
}
void fixtureIconBudget(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}
}
int fixtureIconHostReferences(void){return (int)references;}
