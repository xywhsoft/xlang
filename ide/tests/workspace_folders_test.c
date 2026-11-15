#include "protocol/workspace_folders.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;static int failAfter=-1;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace-folders:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static void* allocation(size_t n,size_t size)
{if(failAfter==0)return NULL;if(failAfter>0)--failAfter;return xrtCalloc(n,size);}
#define xrtCalloc allocation
#include "../src/protocol/workspace_folders.c"
#undef xrtCalloc
static xvalue* object(void){xvalue* v=xrtValueObject();CHECK(v);return v;}
static void put(xvalue* v,const char* k,xvalue* x){CHECK(x&&xrtValueObjectSetNew(v,xrtStrView(k),x));}
static xvalue* row(const char* uri,const char* name)
{xvalue* v=object();put(v,"uri",xrtValueString(xrtStrView(uri)));put(v,"name",xrtValueString(xrtStrView(name)));return v;}
static xvalue* list(const char* uri,const char* name)
{xvalue* a=xrtValueArray();CHECK(a);if(uri){xvalue* f=row(uri,name);CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);}return a;}
static xvalue* event(xvalue* removed,xvalue* added)
{xvalue* p=object(),*e=object();put(e,"removed",removed);put(e,"added",added);put(p,"event",e);return p;}
static xide_workspace_folders* read(xvalue* p)
{xide_model_error e={0};xide_workspace_folders* r=xideWorkspaceFoldersRead(p,&e);CHECK(r&&!e.code);xrtValueRelease(p);return r;}
static void invalid(xvalue* p)
{xide_model_error e={0};CHECK(!xideWorkspaceFoldersRead(p,&e)&&e.code&&*e.message);xrtValueRelease(p);}
static void path(const xide_workspace_folders* r,size_t i,const char* expected,const char* expectedName)
{const char *u=NULL,*n=NULL,*p=NULL;CHECK(xideWorkspaceFoldersAt(r,i,&u,&n,&p)&&u&&n&&p&&xidePathLexicalEqual(p,expected)&&!strcmp(n,expectedName));}
int main(void)
{
 wchar_t before[32768],after[32768];CHECK(GetCurrentDirectoryW(32768,before));
 xide_workspace_folders *r=read(object());CHECK(!xideWorkspaceFoldersCount(r)&&!xideWorkspaceFoldersAt(r,0,NULL,NULL,NULL));xideWorkspaceFoldersRelease(r);
 xvalue* p=object();put(p,"rootPath",xrtValueString(xrtStrView("Z:/not-created/legacy")));r=read(p);path(r,0,"Z:/not-created/legacy","");xideWorkspaceFoldersRelease(r);
 p=object();put(p,"rootPath",xrtValueString(xrtStrView("Z:/wrong")));put(p,"rootUri",xrtValueString(xrtStrView("file:///Z:/not-created/%E4%B8%AD%E6%96%87")));r=read(p);path(r,0,"Z:/not-created/中文","");xideWorkspaceFoldersRelease(r);
 p=object();put(p,"rootPath",xrtValueString(xrtStrView("Z:/wrong")));put(p,"rootUri",xrtValueNull());r=read(p);CHECK(!r->count);xideWorkspaceFoldersRelease(r);
 for(int null=0;null<2;++null){p=object();put(p,"rootUri",xrtValueString(xrtStrView("file:///Z:/wrong")));put(p,"workspaceFolders",null?xrtValueNull():list(NULL,NULL));r=read(p);CHECK(!r->count);xideWorkspaceFoldersRelease(r);}
 p=object();put(p,"rootUri",xrtValueInt(4));put(p,"workspaceFolders",list("file:///Z:/not-created/root","root"));r=read(p);path(r,0,"Z:/not-created/root","root");CHECK(xideWorkspaceFoldersRetain(r)==r);xideWorkspaceFoldersRelease(r);
 const char* bad[]={"https://example.com/root","file:///relative","file:///Z:/bad%00name","file:///Z:/bad%2Fname","file:///Z:/bad%5cname","file:///Z:/bad%ffname","file:///Z:/root#fragment","file:///Z:/root?query",""};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){p=object();put(p,"workspaceFolders",list(bad[i],"bad"));invalid(p);}
 p=object();put(p,"rootPath",xrtValueString(xrtStrView("relative")));invalid(p);
 p=object();put(p,"workspaceFolders",xrtValueInt(1));invalid(p);
 p=object();xvalue* a=list("file:///Z:/same","one"),*f=row("file:///z:/s%61me/","two");CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);put(p,"workspaceFolders",a);invalid(p);
 p=object();a=list(NULL,NULL);f=object();put(f,"uri",xrtValueString(xrtStrView("file:///Z:/missing-name")));CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);put(p,"workspaceFolders",a);invalid(p);
 p=object();a=list(NULL,NULL);f=row("file:///Z:/nul","valid");put(f,"name",xrtValueString((xstrview){"bad\0name",8}));CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);put(p,"workspaceFolders",a);invalid(p);
 int changed=-1;xide_model_error e={0};p=event(list(NULL,NULL),list("file:///z:/not-created/r%6Fot/","renamed"));xide_workspace_folders* next=xideWorkspaceFoldersChange(r,p,&changed,&e);CHECK(next&&!changed&&!e.code);path(next,0,"Z:/not-created/root","renamed");path(r,0,"Z:/not-created/root","root");xideWorkspaceFoldersRelease(next);xrtValueRelease(p);
 p=event(list("file:///Z:/unknown","unknown"),list(NULL,NULL));next=xideWorkspaceFoldersChange(r,p,&changed,&e);CHECK(next&&!changed);xideWorkspaceFoldersRelease(next);xrtValueRelease(p);
 p=event(list("file:///Z:/not-created/root","old"),list("file:///Z:/new","new"));next=xideWorkspaceFoldersChange(r,p,&changed,&e);CHECK(next&&changed&&next->count==1);path(next,0,"Z:/new","new");path(r,0,"Z:/not-created/root","root");xideWorkspaceFoldersRelease(next);xrtValueRelease(p);
 p=event(list("file:///Z:/not-created/root","old"),list("https://invalid/root","bad"));CHECK(!xideWorkspaceFoldersChange(r,p,&changed,&e)&&!changed&&e.code);path(r,0,"Z:/not-created/root","root");xrtValueRelease(p);
 p=event(list("file:///Z:/not-created/root","old"),list(NULL,NULL));next=xideWorkspaceFoldersChange(r,p,&changed,&e);CHECK(next&&changed&&!next->count);xideWorkspaceFoldersRelease(next);xrtValueRelease(p);
 p=event(list(NULL,NULL),list(NULL,NULL));for(int fault=0;fault<2;++fault){failAfter=fault;next=xideWorkspaceFoldersChange(r,p,&changed,&e);failAfter=-1;CHECK(!next&&!changed&&e.code);path(r,0,"Z:/not-created/root","root");}
 next=xideWorkspaceFoldersChange(r,p,&changed,&e);CHECK(next==r&&!changed);xideWorkspaceFoldersRelease(next);xrtValueRelease(p);
 p=event(list(NULL,NULL),list("file:///Z:/another","another"));failAfter=2;next=xideWorkspaceFoldersChange(r,p,&changed,&e);failAfter=-1;CHECK(!next&&!changed&&e.code);path(r,0,"Z:/not-created/root","root");xrtValueRelease(p);
 p=object();failAfter=0;CHECK(!xideWorkspaceFoldersRead(p,&e));failAfter=-1;xrtValueRelease(p);
 char* large=malloc(65537);CHECK(large);memset(large,'a',65536);large[65536]=0;
 p=object();put(p,"workspaceFolders",list("file:///Z:/large",large));invalid(p);large[65535]=0;
 p=object();a=list(NULL,NULL);for(unsigned i=0;i<31;++i){char uri[80];snprintf(uri,sizeof(uri),"file:///Z:/large-%u",i);f=row(uri,large);CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);}put(p,"workspaceFolders",a);
 xide_workspace_folders* many=read(p);CHECK(many->count==31&&many->bytes<XIDE_WORKSPACE_FOLDERS_BYTES);
 p=event(list(NULL,NULL),list("file:///Z:/one-too-many",large));CHECK(!xideWorkspaceFoldersChange(many,p,&changed,&e)&&!changed&&many->count==31);xrtValueRelease(p);xideWorkspaceFoldersRelease(many);free(large);
 p=object();a=list(NULL,NULL);for(unsigned i=0;i<65;++i){char uri[80];snprintf(uri,sizeof(uri),"file:///Z:/root-%u",i);f=row(uri,"root");CHECK(xrtValueArrayAppend(a,f));xrtValueRelease(f);}put(p,"workspaceFolders",a);invalid(p);
 CHECK(GetCurrentDirectoryW(32768,after)&&!wcscmp(before,after));xideWorkspaceFoldersRelease(r);
 printf("workspace_folders_test passed: %u checks; root precedence,immutable metadata,atomic deltas,aliases,UTF8,capacity and OOM.\n",checks);return 0;
}
