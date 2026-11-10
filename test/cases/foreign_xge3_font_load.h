#ifndef FIXTURE_XGE3_FONT_LOAD_H
#define FIXTURE_XGE3_FONT_LOAD_H
#include <xge.h>
#include <xrt_decl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static const char* fixtureFontLoadPath(void)
{const char* path=getenv("XLANG_XGE_FONT_FIXTURE");return path ? path:"";}
static void fixtureFontLoadHeap(size_t* count,size_t* bytes)
{xmemdebugsnapshot s;memset(&s,0,sizeof(s));xrtMemDebugSnapshot(&s);*count=s.LiveCount;*bytes=s.LiveBytes;}
static int fixtureFontLoadNativeCosts(void)
{
 xge_async_owned request=NULL;xmemdebugsnapshot a,b;memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));xrtMemDebugSnapshot(&a);
 int status=xgeAsyncFontCreateOwned(&request,NULL);if(status)return status;
 xrtMemDebugSnapshot(&b);printf("Native empty FontLoad: %llu allocations / %llu bytes\n",
  (unsigned long long)(b.LiveCount-a.LiveCount),(unsigned long long)(b.LiveBytes-a.LiveBytes));
 return xgeAsyncLoadReleaseChecked(request);
}
#endif
