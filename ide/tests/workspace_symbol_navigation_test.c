#include "services/workspace_symbols.h"
#include "protocol/workspace_symbol_wire.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbol_navigation_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static const xvalue* get(const xvalue* v,const char* key){return xrtValueObjectGet(v,xrtStrView(key));}
static int textIs(const xvalue* v,const char* expected){xstrview text={0};return xrtValueGetString(v,&text) && xrtStrEqual(text,xrtStrView(expected));}
static int pathIs(const xvalue* v,const char* expected){xstrview text={0};return xrtValueGetString(v,&text) && xidePathLexicalEqual(text.Data,expected);}
static int writeText(const char* p,const char* text){return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)text,strlen(text)});}
static xvalue* finish(xide_workspace_symbol_query* q,int ready)
{
 enum xide_workspace_symbol_state state=XIDE_WORKSPACE_SYMBOL_PENDING;char error[512]={0};
 for(unsigned spin=0;q && spin<30000 && state==XIDE_WORKSPACE_SYMBOL_PENDING;++spin){state=xideWorkspaceSymbolsResult(q,NULL,NULL,error,sizeof(error));if(state==XIDE_WORKSPACE_SYMBOL_PENDING)xrtSleep(1);}
 xvalue* out=xideWorkspaceSymbolsTakeLsp(q);if((state==XIDE_WORKSPACE_SYMBOL_READY)!=ready || (out!=NULL)!=ready)fprintf(stderr,"navigation unexpected state%d expected%d:%s\n",state,ready,error);
 if(!ready && state!=XIDE_WORKSPACE_SYMBOL_FAILED && state!=XIDE_WORKSPACE_SYMBOL_CANCELLED){xrtValueRelease(out);out=xrtValueNull();}
 xideWorkspaceSymbolsQueryDestroy(q);return out;
}
static xvalue* navigate(xide_workspace_symbols* store,xls_context* const* contexts,size_t count,const xls_analysis_document* doc,const xvalue* selection,int ready)
{return finish(xideWorkspaceSymbolsSubmitNavigation(store,contexts,count,doc,doc?1:0,"Payload number",200,0,selection),ready);}
int main(void)
{
 char *directory=xrtPathAbs("build/workspace-symbol-navigation/中文😀"),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe");CHECK(directory && entry && helper && compiler && xrtDirCreateAll(directory));
 const char* mainBody="import helper\nfunc main(...) as int\n return helper.value()\nend\n";
 const char* body="\xef\xbb\xbf/* 中文😀 */ class Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 1\r\nend\r\n";
 const char* dirty="\xef\xbb\xbf/* 中文😀 */ class Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 2\r\nend\r\n";
 CHECK(writeText(entry,mainBody) && writeText(helper,body));const char* members[]={helper};xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_context* contexts[]={context};xide_toolchains* cache=xideToolchainsCreate();xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(cache,compiler,NULL);CHECK(context && cache && store);
 xvalue* rows=finish(xideWorkspaceSymbolsSubmitLsp(store,contexts,1,NULL,0,"Payload number",200,0),1);CHECK(rows && xrtValueCount(rows)==1);xvalue* selection=xrtValueRetain(xrtValueArrayGet(rows,0));CHECK(xideWorkspaceSymbolSelectionValid(selection));xrtValueRelease(rows);
 xvalue* proof=navigate(store,contexts,1,NULL,selection,1);CHECK(proof && xrtValueEqual(get(proof,"symbol"),selection));const xvalue* document=get(proof,"document");CHECK(textIs(get(document,"origin"),"disk") && pathIs(get(document,"path"),helper) && !get(document,"version") && get(document,"input") && !get(document,"diagnostics"));xvalue* originalDigest=xrtValueRetain(get(document,"normalizedSha256"));CHECK(originalDigest);xrtValueRelease(proof);
 CHECK(writeText(helper,dirty));proof=navigate(store,contexts,1,NULL,selection,1);CHECK(proof && !xrtValueEqual(get(get(proof,"document"),"normalizedSha256"),originalDigest));xrtValueRelease(proof);
 xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);xls_analysis_document doc={"helper",helper,text,0,0};CHECK(text);
 for(unsigned round=0;round<3;++round){doc.revision=round==2?UINT32_MAX:round;proof=navigate(store,contexts,1,&doc,selection,1);CHECK(proof && xrtValueEqual(get(proof,"symbol"),selection));document=get(proof,"document");int64 version=0;CHECK(textIs(get(document,"origin"),"overlay") && !get(document,"input") && xrtValueGetInt(get(document,"version"),&version) && version==(int64)INT_MIN+(int64)doc.revision && !xrtValueEqual(get(document,"normalizedSha256"),originalDigest));xrtValueRelease(proof);}
 /* The editor overlay includes the BOM scalar;disk normalization strips it. */
 xlsTextRelease(text);doc.text=NULL;proof=navigate(store,contexts,1,NULL,selection,1);CHECK(proof && textIs(get(get(proof,"document"),"origin"),"disk"));xrtValueRelease(proof);
 xvalue* changed=xrtValueDeepClone(selection);CHECK(changed && xrtValueObjectSetNew(changed,xrtStrView("name"),xrtValueString(xrtStrView("amount"))) && xideWorkspaceSymbolSelectionValid(changed));CHECK(!navigate(store,contexts,1,NULL,changed,0));xrtValueRelease(changed);
 changed=xrtValueDeepClone(selection);CHECK(changed && xrtValueObjectSetNew(changed,xrtStrView("kind"),xrtValueInt(12)) && xideWorkspaceSymbolSelectionValid(changed));CHECK(!navigate(store,contexts,1,NULL,changed,0));xrtValueRelease(changed);
 changed=xrtValueDeepClone(selection);CHECK(changed && xrtValueObjectSetNew(changed,xrtStrView("containerName"),xrtValueString(xrtStrView("Other"))) && xideWorkspaceSymbolSelectionValid(changed));CHECK(!navigate(store,contexts,1,NULL,changed,0));xrtValueRelease(changed);
 changed=xrtValueDeepClone(selection);xvalue* start=xrtValueObjectGet(xrtValueObjectGet(xrtValueObjectGet(changed,xrtStrView("location")),xrtStrView("range")),xrtStrView("start"));CHECK(start && xrtValueObjectSetNew(start,xrtStrView("character"),xrtValueInt(0)) && xideWorkspaceSymbolSelectionValid(changed));CHECK(!navigate(store,contexts,1,NULL,changed,0));xrtValueRelease(changed);
 CHECK(!finish(xideWorkspaceSymbolsSubmitNavigation(store,contexts,1,NULL,0,"main",200,0,selection),0));
 CHECK(writeText(helper,"class Payload\n int amount\nend\nexport func value() as int\n return 1\nend\n"));CHECK(!navigate(store,contexts,1,NULL,selection,0));
 CHECK(writeText(helper,body));proof=navigate(store,contexts,1,NULL,selection,1);CHECK(proof);xrtValueRelease(proof);
 CHECK(!xideWorkspaceSymbolsSubmitNavigation(store,contexts,1,NULL,0,"",200,0,NULL));
 xide_workspace_symbol_query* cancelled=xideWorkspaceSymbolsSubmitNavigation(store,contexts,1,NULL,0,"Payload number",200,0,selection);CHECK(cancelled);xideWorkspaceSymbolsCancel(cancelled);CHECK(!finish(cancelled,0));
 xrtValueRelease(selection);xrtValueRelease(originalDigest);xideWorkspaceSymbolsDestroy(store);xlsContextRelease(context);xideToolchainsDestroy(cache);xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(compiler);printf("workspace_symbol_navigation_test passed: %u checks;ordinary production API,fresh disk/BOM/CRLF/Unicode and overlay proofs,full int32 versions,exact selection matching,changed rows and cancellation.\n",checks);return 0;
}
