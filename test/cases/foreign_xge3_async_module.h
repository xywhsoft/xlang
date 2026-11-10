#ifndef FIXTURE_XGE3_ASYNC_MODULE_H
#define FIXTURE_XGE3_ASYNC_MODULE_H
#include <xge.h>
int fixtureAsyncModuleTick(void);
int fixtureAsyncModuleCallbacks(void);
const char* fixtureAsyncModulePath(void);
int fixtureAsyncModulePublish(xge_async_owned request);
#endif
