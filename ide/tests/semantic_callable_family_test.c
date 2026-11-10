#include <xrt.h>
/* Only track allocations during this demand-only API, after real checking. */
static _Thread_local size_t callableAllocationCount,callableAllocationFault,callableRequested[2];
static _Thread_local int callableTracking;
void* __real_xrtMalloc(size_t);
void* __wrap_xrtMalloc(size_t bytes)
{
 if(callableTracking){size_t at=callableAllocationCount++;if(at<2)callableRequested[at]=bytes;if(callableAllocationCount==callableAllocationFault)return NULL;}
 return __real_xrtMalloc(bytes);
}
#define main dispatchFamilyOriginalMain
#include "semantic_dispatch_family_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_callable_family_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
#include "analysis/callable.h"
static int related(family_owner* o,const char* path,const char* text,const char* seed,const char* name,size_t members,unsigned kinds)
{
 xide_reference_key* key=familyKeyAt(o,path,text,seed);CHECK(key);enum xide_reference_family_error why;
 xide_reference_family* f=makeFamily(o,key,0,NULL,NULL,&why);if(!f)fprintf(stderr,"family error=%d seed=%s\n",why,seed);
 CHECK(f && familyKeys(o,f,name,members,1));xide_reference_family_info info;CHECK(xideSemanticReferenceFamilyInfo(f,&info) && info.kinds==kinds);
 xideSemanticReferenceKeyRelease(key);xideSemanticReferenceFamilyRelease(f);return 1;
}
static int callableEqual(xvalue* a,xvalue* b)
{int same=a && b && xrtValueEqual(a,b);xrtValueRelease(a);xrtValueRelease(b);return same;}
static int callableParity(family_owner* o)
{
 const char* json=xlsSemanticCheckIndex(o->check);CHECK(json);xvalue* value=xrtJsonParse(xrtStrView(json));CHECK(value);xide_semantic_definitions* compat=xideSemanticDefinitionsCreate(value,o->sources,o->count,NULL);xrtValueRelease(value);
 xide_semantic_definitions* typed=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(o->check),o->sources,o->count,NULL);xide_navigation_snapshot* points=xideSemanticNavigationCapture(o->check,o->sources,o->count,NULL);CHECK(compat && typed && points && xideSemanticNavigationPreparePoints(points,NULL)==1);
 for(size_t i=0;i<o->count;++i){const xide_semantic_source* s=o->sources+i;size_t bytes=strlen(s->text);for(size_t at=0;at<bytes;++at){if(((unsigned char)s->text[at]&0xc0)==0x80)continue;int line,column;if(!xideLspOffsetToPosition(s->text,(int)at,&line,&column))continue;if(!line)column+=s->utf16Prefix;
  CHECK(callableEqual(xideSemanticDefinition(typed,s->path,line,column),xideSemanticDefinition(compat,s->path,line,column)));CHECK(callableEqual(xideSemanticDefinition(typed,s->path,line,column),xideSemanticNavigationPointDefinition(points,s->path,line,column)));
 }}
 xideSemanticDefinitionsDestroy(compat);xideSemanticDefinitionsDestroy(typed);xideSemanticNavigationDestroy(points);return 1;
}
static int callableFamilies(void)
{
 char* directory=xrtPathAbs("build/callable-families");CHECK(directory && xrtDirCreateAll(directory));char* path=xrtPathJoin(directory,"入口😀.xl");CHECK(path);
 const char* raw="func choose(int n) as int\n return n\nend\nfunc choose(int n,int m) as int\n return n+m\nend\n"
 "class Box\n int data\n func choose(int n) as int\n return n\n end\n func choose(int n,int m) as int\n return n+m\n end\n get size() as int\n return self.data\n end\n let size(int value)\n self.data=value\n end\nend\n"
 "class Other\n func choose(int n) as int\n return n\n end\nend\n"
 "protocol Reader\n func read() as int\nend\n"
 "class Base\n virtual func read() as int\n return 1\n end\n func read(int n) as int\n return n\n end\nend\n"
 "class Child extends Base\n override func read() as int\n return 2\n end\nend\n"
 "func through(Reader value) as int\n return value.read()\nend\n"
 "func main(...) as int\n auto box=Box()\n box.size=3\n return box.size+box.choose(2)+box.choose(2,3)+choose(1)+choose(1,2)+Other().choose(4)+through(Child())\nend\n";
 xls_semantic_input input={path,raw,strlen(raw)};family_owner o={0};CHECK(captureFamily(&o,path,&input,1));
 CHECK(related(&o,path,raw,"choose(int n)","choose",2,XLS_CALLABLE_OVERLOAD));
 CHECK(related(&o,path,raw,"choose(int n,int m)","choose",2,XLS_CALLABLE_OVERLOAD));
 const char* box=strstr(raw,"class Box");CHECK(box);CHECK(related(&o,path,raw,strstr(box,"choose(int n)"),"choose",2,XLS_CALLABLE_OVERLOAD));
 CHECK(related(&o,path,raw,"size() as int","size",2,XLS_CALLABLE_PROPERTY));
 CHECK(related(&o,path,raw,"size(int value)","size",2,XLS_CALLABLE_PROPERTY));
 const char* other=strstr(raw,"class Other");CHECK(other && related(&o,path,raw,strstr(other,"choose(int n)"),"choose",1,0));
 CHECK(related(&o,path,raw,"read() as int","read",4,XLS_DISPATCH_CLASS|XLS_DISPATCH_PROTOCOL|XLS_CALLABLE_OVERLOAD));CHECK(callableParity(&o));
 enum xls_callable_error why;callableTracking=1;callableAllocationCount=0;xls_callable_relations* groups=xlsSemanticCheckCallableRelations(o.check,0,NULL,NULL,&why);callableTracking=0;
 CHECK(groups && why==XLS_CALLABLE_OK && xlsCallableRelationsCount(groups)>=4 && xlsCallableRelationsBytes(groups));CHECK(callableAllocationCount==2 && callableRequested[1]==xlsCallableRelationsBytes(groups));size_t concurrent=callableRequested[0]+callableRequested[1];
 for(size_t i=1;i<=2;++i){uint64_t before=blocks();callableTracking=1;callableAllocationCount=0;callableAllocationFault=i;xls_callable_relations* failed=xlsSemanticCheckCallableRelations(o.check,0,NULL,NULL,&why);callableTracking=0;callableAllocationFault=0;CHECK(!failed && why==XLS_CALLABLE_MEMORY && callableAllocationCount==i && blocks()==before);}
 xls_callable_relation row={.kind=9};CHECK(!xlsCallableRelationsAt(groups,xlsCallableRelationsCount(groups),&row) && !row.kind);CHECK(!xlsSemanticCheckCallableRelations(NULL,0,NULL,NULL,&why) && why==XLS_CALLABLE_INVALID);
 CHECK(!xlsSemanticCheckCallableRelations(o.check,1,NULL,NULL,&why) && why==XLS_CALLABLE_LIMIT);
 size_t low=xlsCallableRelationsBytes(groups),high=low+65536;while(low<high){size_t mid=low+(high-low)/2;xls_callable_relations* test=xlsSemanticCheckCallableRelations(o.check,mid,NULL,NULL,&why);if(test){high=mid;xlsCallableRelationsDestroy(test);}else{CHECK(why==XLS_CALLABLE_LIMIT);low=mid+1;}}
 CHECK(low==concurrent);xls_callable_relations* exact=xlsSemanticCheckCallableRelations(o.check,low,NULL,NULL,&why);CHECK(exact);xlsCallableRelationsDestroy(exact);CHECK(!xlsSemanticCheckCallableRelations(o.check,low-1,NULL,NULL,&why) && why==XLS_CALLABLE_LIMIT);
 cancellation c={0,SIZE_MAX};exact=xlsSemanticCheckCallableRelations(o.check,0,cancelFamily,&c,&why);CHECK(exact && c.polls>8);xlsCallableRelationsDestroy(exact);size_t polls=c.polls;
 for(size_t i=0;i<polls;++i){uint64_t before=blocks();c=(cancellation){0,i};CHECK(!xlsSemanticCheckCallableRelations(o.check,0,cancelFamily,&c,&why) && why==XLS_CALLABLE_CANCELLED && c.polls==i+1 && blocks()==before);}
 clearFamily(&o);for(size_t i=0;i<xlsCallableRelationsCount(groups);++i)CHECK(xlsCallableRelationsAt(groups,i,&row) && row.declaration && row.implementation && row.kind);xlsCallableRelationsDestroy(groups);
 xstrbuf unicode={0};CHECK(xrtStrBufAppend(&unicode,xrtStrView("\xef\xbb\xbf")));for(const char* at=raw;*at;){if(!strncmp(at,"choose",6)){CHECK(xrtStrBufAppend(&unicode,xrtStrView("挑选😀")));at+=6;}else if(!strncmp(at,"size",4)){CHECK(xrtStrBufAppend(&unicode,xrtStrView("尺寸😀")));at+=4;}else if(*at=='\n'){CHECK(xrtStrBufAppend(&unicode,xrtStrView("\r\n")));++at;}else{CHECK(xrtStrBufAppend(&unicode,(xstrview){at,1}));++at;}}
 input=(xls_semantic_input){path,unicode.Data,unicode.Size};CHECK(captureFamily(&o,path,&input,1));CHECK(related(&o,path,unicode.Data,"挑选😀(int n)","挑选😀",2,XLS_CALLABLE_OVERLOAD));CHECK(related(&o,path,unicode.Data,"尺寸😀() as int","尺寸😀",2,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,unicode.Data,"尺寸😀(int value)","尺寸😀",2,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,unicode.Data,"尺寸😀=3","尺寸😀",2,XLS_CALLABLE_PROPERTY));CHECK(callableParity(&o));clearFamily(&o);xrtStrBufFree(&unicode);
 const char* indexed="class Indexed\n int data\n get item() as int\n return self.data\n end\n get item(int index) as int\n return self.data+index\n end\n let item(int value)\n self.data=value\n end\n let item(int index,int value)\n self.data=value+index\n end\nend\nfunc main(...) as int\n auto box=Indexed()\n int at=2\n int value=3\n box.item=1\n box.item[at]=value\n box.item[at]+=value\n return box.item+box.item[at]\nend\n";
 input=(xls_semantic_input){path,indexed,strlen(indexed)};CHECK(captureFamily(&o,path,&input,1));CHECK(related(&o,path,indexed,"item() as int","item",4,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,indexed,"item(int index,int value)","item",4,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,indexed,"item[at]=value","item",4,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,indexed,"item[at]+=value","item",4,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,indexed,"item[at]\nend","item",4,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,indexed,"at]=value","at",1,0));CHECK(related(&o,path,indexed,"value\n box.item[at]+=","value",1,0));CHECK(callableParity(&o));clearFamily(&o);
 char* dependency=xrtPathJoin(directory,"dep.xl");CHECK(dependency);const char* lib="\xef\xbb\xbf" "export func choose(int value) as int\r\n return value\r\nend\r\nexport func choose(int first,int second) as int\r\n return first+second\r\nend\r\nexport class Box\r\n int data\r\n get size() as int\r\n return self.data\r\n end\r\n let size(int value)\r\n self.data=value\r\n end\r\nend\r\n";
 const char* imported="import dep\nfunc main(...) as int\n auto box=dep.Box()\n box.size=2\n return dep.choose(1)+dep.choose(2,3)+box.size\nend\n";xls_semantic_input inputs[]={{path,imported,strlen(imported)},{dependency,lib,strlen(lib)}};CHECK(captureFamily(&o,path,inputs,2));CHECK(related(&o,path,imported,"choose(1)","choose",2,XLS_CALLABLE_OVERLOAD));CHECK(related(&o,path,imported,"size=2","size",2,XLS_CALLABLE_PROPERTY));CHECK(related(&o,path,imported,"box.size=2","box",1,0));CHECK(callableParity(&o));clearFamily(&o);
 const char* writeOnly="class Sink\n int data\n let output(int value)\n self.data=value\n end\nend\nfunc main(...) as int\n auto box=Sink()\n box.output=3\n return 0\nend\n";input=(xls_semantic_input){path,writeOnly,strlen(writeOnly)};CHECK(captureFamily(&o,path,&input,1));CHECK(related(&o,path,writeOnly,"output=3","output",1,0));CHECK(callableParity(&o));clearFamily(&o);
 xrtFree(dependency);xrtFree(path);xrtFree(directory);return 1;
}
int main(void){if(!callableFamilies())return 1;printf("semantic_callable_family_test passed: %u checks; original native lexical overload/property associations, distinct owners, dispatch transitivity, exact quota, cancellation and owner-free rows\n",checks);return 0;}
