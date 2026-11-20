#ifndef XL6_FIXTURE_XGE_ASYNC_LOAD_H
#define XL6_FIXTURE_XGE_ASYNC_LOAD_H
#include <xrt_decl.h>
#include <string.h>
#include "foreign_xge3_async_file.h"
static void fixtureAsyncHeap(size_t* count,size_t* bytes)
{xmemdebugsnapshot info;memset(&info,0,sizeof(info));xrtMemDebugSnapshot(&info);*count=info.LiveCount;*bytes=info.LiveBytes;}
static void fixtureAsyncFailAfter(int budget){if(!xrtMemDebugFailAfter(budget))abort();}
static void fixtureAsyncFailClear(void){xrtMemDebugFailClear();}
#endif
