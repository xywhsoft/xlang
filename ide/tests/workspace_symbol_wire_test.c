#include "protocol/workspace_symbol_wire.h"
#include "protocol/lsp_wire.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture_rows {size_t count;const char *path,*name,*qualified;int kind;} fixture_rows;
static size_t fixtureCount(const xide_workspace_symbol_result* p){return ((const fixture_rows*)p)->count;}
static int fixtureAt(const xide_workspace_symbol_result* p,size_t at,xide_workspace_symbol_view* out)
{const fixture_rows* rows=(const fixture_rows*)p;if(at>=rows->count)return 0;*out=(xide_workspace_symbol_view){.qualified=rows->qualified,.declaration={.path=rows->path,.name=rows->name,.kind=rows->kind,.selectionStart={(int)at,1,0},.selectionEnd={(int)at,7,0}}};return 1;}
static size_t operations,faultAt,paths;static int tracking;
static int operation(void){return !tracking || ++operations!=faultAt;}
static xvalue* valueObject(void){return operation()?xrtValueObject():NULL;}
static xvalue* valueArray(void){return operation()?xrtValueArray():NULL;}
static xvalue* valueInt(int64 n){return operation()?xrtValueInt(n):NULL;}
static xvalue* valueString(xstrview s){return operation()?xrtValueString(s):NULL;}
static bool objectSet(xvalue* o,xstrview key,xvalue* v){if(operation())return xrtValueObjectSetNew(o,key,v);xrtValueRelease(v);return false;}
static bool arrayAppend(xvalue* o,xvalue* v){if(operation())return xrtValueArrayAppendNew(o,v);xrtValueRelease(v);return false;}
static char* pathUri(const char* path,xide_model_error* e){char* p=operation()?xideLspPathToUri(path,e):NULL;if(p)++paths;return p;}
static void pathFree(void* p){if(p)--paths;free(p);}
static int writerActive,writerPolls;
static bool observedWrite(const xvalue* value,const xjsonwriteconfig* config,bool (*write)(xbytesview,void*),void* user)
{writerActive=1;bool result=xrtJsonWrite(value,config,write,user);writerActive=0;return result;}
static int stopInWriter(void* p){(void)p;if(writerActive)++writerPolls;return writerPolls>=5;}
#define xideWorkspaceSymbolResultCount fixtureCount
#define xideWorkspaceSymbolResultAt fixtureAt
#define xrtValueObject valueObject
#define xrtValueArray valueArray
#define xrtValueInt valueInt
#define xrtValueString valueString
#define xrtValueObjectSetNew objectSet
#define xrtValueArrayAppendNew arrayAppend
#define xideLspPathToUri pathUri
#define xrtJsonWrite observedWrite
#define free pathFree
#include "../src/protocol/workspace_symbol_wire.c"
#undef xideWorkspaceSymbolResultCount
#undef xideWorkspaceSymbolResultAt
#undef xrtValueObject
#undef xrtValueArray
#undef xrtValueInt
#undef xrtValueString
#undef xrtValueObjectSetNew
#undef xrtValueArrayAppendNew
#undef xideLspPathToUri
#undef xrtJsonWrite
#undef free
static size_t checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbol_wire_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static const xvalue* field(const xvalue* p,const char* key){return xrtValueObjectGet(p,xrtStrView(key));}
static int stopAt(void* p){size_t* state=p;return ++state[0]>=state[1];}
static uint64_t blocks(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
int main(void)
{
 fixture_rows rows={2,"D:/workspace/中文😀.xl","number","Payload.number",8};const xide_workspace_symbol_result* result=(const xide_workspace_symbol_result*)&rows;xide_model_error error={0};
 xvalue* json=xideWorkspaceSymbolWire(result,0,NULL,NULL,&error);CHECK(json && xrtValueCount(json)==2 && !error.code);const xvalue* item=xrtValueArrayGet(json,0);xstrview name={0},container={0},uri={0};int64 kind=0;
 CHECK(xrtValueGetString(field(item,"name"),&name) && xrtStrEqual(name,xrtStrView("number")) && xrtValueGetString(field(item,"containerName"),&container) && xrtStrEqual(container,xrtStrView("Payload")) && xrtValueGetInt(field(item,"kind"),&kind) && kind==8);
 CHECK(xideWorkspaceSymbolSelectionValid(item) && !xideWorkspaceSymbolSelectionValid(NULL));
 xvalue* invalid=xrtValueDeepClone(item);CHECK(invalid && xrtValueObjectSetNew(invalid,xrtStrView("kind"),xrtValueBool(true)) && !xideWorkspaceSymbolSelectionValid(invalid));xrtValueRelease(invalid);
 invalid=xrtValueDeepClone(item);CHECK(invalid && xrtValueObjectSetNew(invalid,xrtStrView("extra"),xrtValueArray()) && !xideWorkspaceSymbolSelectionValid(invalid));xrtValueRelease(invalid);
 invalid=xrtValueDeepClone(item);CHECK(invalid && xrtValueObjectSetNew(invalid,xrtStrView("name"),xrtValueString((xstrview){"n\0m",3})) && !xideWorkspaceSymbolSelectionValid(invalid));xrtValueRelease(invalid);
 CHECK(xrtValueGetString(field(field(item,"location"),"uri"),&uri));char* path=xideLspUriToPath(uri.Data,NULL);CHECK(path && !strcmp(path,rows.path));free(path);const xvalue* start=field(field(field(item,"location"),"range"),"start");CHECK(xrtValueGetInt(field(start,"line"),&kind) && kind==0 && xrtValueGetInt(field(start,"character"),&kind) && kind==1);xrtValueRelease(json);
 rows.kind=26;json=xideWorkspaceSymbolWire(result,0,NULL,NULL,&error);CHECK(json && xrtValueGetInt(field(xrtValueArrayGet(json,0),"kind"),&kind) && kind==13);xrtValueRelease(json);
 json=xideWorkspaceSymbolWire(result,UINT32_C(1)<<25,NULL,NULL,&error);CHECK(json && xrtValueGetInt(field(xrtValueArrayGet(json,0),"kind"),&kind) && kind==26);xrtValueRelease(json);
 json=xideWorkspaceSymbolWire(result,UINT32_C(1)<<4,NULL,NULL,&error);CHECK(json && xrtValueGetInt(field(xrtValueArrayGet(json,0),"kind"),&kind) && kind==5);xrtValueRelease(json);
 tracking=1;operations=faultAt=0;json=xideWorkspaceSymbolWire(result,0,NULL,NULL,&error);tracking=0;CHECK(json);size_t count=operations;xrtValueRelease(json);uint64_t before=blocks();
 for(size_t at=1;at<=count;++at){tracking=1;operations=0;faultAt=at;json=xideWorkspaceSymbolWire(result,0,NULL,NULL,&error);tracking=0;CHECK(!json && error.code && operations>=at && !paths && blocks()==before);}
 size_t stop[2]={0,SIZE_MAX};json=xideWorkspaceSymbolWire(result,0,stopAt,stop,&error);CHECK(json);xrtValueRelease(json);size_t gates=stop[0];
 for(size_t at=1;at<=gates;++at){stop[0]=0;stop[1]=at;json=xideWorkspaceSymbolWire(result,0,stopAt,stop,&error);CHECK(!json && error.code==XIDE_MODEL_CONFLICT && !paths && blocks()==before);}
 writerPolls=0;json=xideWorkspaceSymbolWire(result,0,stopInWriter,NULL,&error);CHECK(!json && writerPolls>=5 && error.code==XIDE_MODEL_CONFLICT && !paths && blocks()==before);
 char* longPath=xrtMalloc(2400);CHECK(longPath);memcpy(longPath,"D:/",3);memset(longPath+3,' ',2390);memcpy(longPath+2393,"/a.xl",6);rows.path=longPath;rows.count=4096;json=xideWorkspaceSymbolWire(result,0,NULL,NULL,&error);CHECK(!json && error.code && !paths);xrtFree(longPath);CHECK(blocks()==before);
 printf("workspace_symbol_wire_test passed: %zu checks;%zu real writer-boundary refusal points;%zu cancellation gates;standard names/container/kind/UTF16 URI locations and escaped7MiB overflow. Synthetic encoding rows,not semantic analysis.\n",checks,count,gates);return 0;
}
