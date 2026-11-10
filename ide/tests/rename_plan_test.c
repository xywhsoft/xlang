#include <xrt.h>
#include <stdio.h>
#include <string.h>
static size_t fault=SIZE_MAX,allocations,cloneFault=SIZE_MAX,clones;
static void* allocatePlan(size_t n){return allocations++==fault ? NULL : xrtMalloc(n);}
static xvalue* clonePlan(const xvalue* v){return clones++==cloneFault ? NULL : xrtValueDeepClone(v);}
#define XIDE_RENAME_PLAN_ALLOC allocatePlan
#define xrtValueDeepClone clonePlan
#include "../src/services/rename_plan.c"
#undef xrtValueDeepClone
#include "services/language_check.h"
#include "analysis/text.h"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"rename_plan_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static const xvalue* get(const xvalue* v,const char* key){return xideReferenceDocumentField(v,key);}
static int set(xvalue* v,const char* key,xvalue* child){return child && xrtValueObjectSetNew(v,xrtStrView(key),child);}
static xvalue* proofFor(const xide_semantic_source* source,int version,int bom,int disk)
{
 if(!disk)return xideLanguageReferenceDocument(source,version,NULL);
 xsha256 hash;xrtSha256Init(&hash);if(bom && !xrtSha256Update(&hash,"\xef\xbb\xbf",3))return NULL;unsigned char digest[32];if(!xrtSha256Update(&hash,source->text,strlen(source->text)) || !xrtSha256Final(&hash,digest))return NULL;char hex[65];for(size_t i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",digest[i]);xvalue* input=xrtValueObject();int okay=input && set(input,"path",xrtValueString(xrtStrView(source->path))) && set(input,"origin",xrtValueString(xrtStrView("disk"))) && set(input,"rawSize",xrtValueInt((int64)strlen(source->text)+3*bom)) && set(input,"rawSha256",xrtValueString(xrtStrView(hex)));xvalue* proof=okay ? xideLanguageReferenceDocument(source,version,input) : NULL;xrtValueRelease(input);return proof;
}
static int changeAt(const xide_semantic_source* source,size_t document,size_t at,xide_rename_plan_change* out)
{
 int line,column,endLine,endColumn;if(!xideLspOffsetToPosition(source->text,(int)at,&line,&column) || !xideLspOffsetToPosition(source->text,(int)at+3,&endLine,&endColumn))return 0;
 *out=(xide_rename_plan_change){document,{line,column+(line ? 0 : source->utf16Prefix),(int)at},{endLine,endColumn+(endLine ? 0 : source->utf16Prefix),(int)at+3}};return 1;
}
typedef struct cancellation {size_t polls,at;} cancellation;
static int cancelPlan(void* user){cancellation* c=user;return c->polls++==c->at;}
static xide_rename_plan* create(const xide_rename_plan_input* docs,const xide_rename_plan_change* edits,const char* name,size_t limit,int (*stop)(void*),void* user,enum xide_rename_plan_error* why)
{return xideRenamePlanCreate(docs,2,edits,5,xrtStrView("foo"),xrtStrView(name),limit,stop,user,why);}
static int oracle(xide_rename_plan* plan,const char* replacement)
{
 xide_rename_plan_info info;CHECK(xideRenamePlanInfo(plan,&info) && info.documents==2 && info.edits==5 && info.bytes && info.proofBytes);
 for(size_t i=0;i<info.documents;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d));size_t bom=d.before.Size>=3 && !memcmp(d.before.Data,"\xef\xbb\xbf",3) ? 3 : 0;const char* original=d.before.Data+bom;size_t bytes=d.before.Size-bom;xls_text* before=xlsTextCreate(original,bytes,8u*1024u*1024u,NULL);CHECK(before);xls_text_edit changes[5]={0};
  for(size_t j=0;j<d.count;++j){xide_rename_plan_edit e;CHECK(xideRenamePlanEditAt(plan,d.first+d.count-1-j,&e));xide_lsp_position start=e.start,end=e.end;if(!start.line)start.character-=d.coordinatePrefix;if(!end.line)end.character-=d.coordinatePrefix;changes[j]=(xls_text_edit){replacement,strlen(replacement),{(size_t)start.line,(size_t)start.character},{(size_t)end.line,(size_t)end.character},1,0,0};size_t mapped=0;CHECK(xideRenamePlanMapOffset(plan,i,(size_t)e.start.offset,&mapped) && mapped==e.afterStart);CHECK(xideRenamePlanMapOffset(plan,i,(size_t)e.end.offset,&mapped) && mapped==e.afterEnd);CHECK(!xideRenamePlanMapOffset(plan,i,(size_t)e.start.offset+1,&mapped));}
  xls_text* after=xlsTextApply(before,changes,d.count,8u*1024u*1024u,NULL);CHECK(after && d.after.Size>=bom && (!bom || !memcmp(d.after.Data,"\xef\xbb\xbf",3)) && xlsTextSize(after)==d.after.Size-bom && !memcmp(xlsTextData(after),d.after.Data+bom,xlsTextSize(after)));
  size_t mapped=0;CHECK(xideRenamePlanMapOffset(plan,i,bytes,&mapped) && mapped==d.after.Size-bom);CHECK(!xideRenamePlanMapOffset(plan,i,bytes+1,&mapped));xlsTextRelease(before);xlsTextRelease(after);
 }
 for(int guarded=0;guarded<2;++guarded){xvalue* wire=xideRenamePlanWorkspaceEdit(plan,guarded,NULL,NULL);const xvalue* docs=get(wire,"documentChanges");CHECK(wire && xrtValueCount(docs)==2);for(size_t i=0;i<2;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d));const xvalue* row=xrtValueArrayGet(docs,i),*document=get(row,"textDocument");CHECK(xrtValueCount(get(row,"edits"))==d.count && (!!get(row,"xlang"))==guarded);if(d.hasVersion){int64 version=0;CHECK(xrtValueGetInt(get(document,"version"),&version) && version==INT_MAX);}else CHECK(xrtValueType(get(document,"version"))==XVALUE_NULL);}
  xrtValueRelease(wire);
 }return 1;
}
typedef struct reader {xide_rename_plan* plan;int okay;} reader;
static int32 readPlan(void* user){reader* r=user;r->okay=1;for(size_t n=0;n<100;++n){xide_rename_plan_document d;xide_rename_plan_edit e;if(!xideRenamePlanDocumentAt(r->plan,n%2,&d) || !xideRenamePlanEditAt(r->plan,n%5,&e) || !d.proof || !d.before.Data || !d.after.Data)r->okay=0;}xideRenamePlanRelease(r->plan);return 0;}
static int wireLimit(void)
{
 char* path=xrtPathAbs("build/rename-plan/large.xl");char* text=xrtMalloc(801);char* name=xrtMalloc(40001);CHECK(path && text && name);for(size_t i=0;i<200;++i)memcpy(text+4*i,"foo ",4);text[800]=0;memset(name,'z',40000);name[40000]=0;xide_semantic_source source={path,text,1,0};xvalue* proof=proofFor(&source,1,0,0);CHECK(proof);xide_rename_plan_input input={source,proof};xide_rename_plan_change edits[200];for(size_t i=0;i<200;++i)edits[i]=(xide_rename_plan_change){0,{0,(int)(4*i),(int)(4*i)},{0,(int)(4*i+3),(int)(4*i+3)}};enum xide_rename_plan_error why;xide_rename_plan* plan=xideRenamePlanCreate(&input,1,edits,200,xrtStrView("foo"),xrtStrView(name),0,NULL,NULL,&why);CHECK(plan && why==XIDE_RENAME_PLAN_OK);uint64_t before=blocks();CHECK(!xideRenamePlanWorkspaceEdit(plan,0,NULL,NULL) && blocks()==before);xideRenamePlanRelease(plan);xrtValueRelease(proof);xrtFree(path);xrtFree(text);xrtFree(name);return 1;
}
static int run(void)
{
 char *a=xrtPathAbs("build/rename-plan/a.xl"),*b=xrtPathAbs("build/rename-plan/源码😀.xl");CHECK(a && b);char *left=xrtStrDup("foo foo\r\nfoo"),*right=xrtStrDup("func foo() as int\r\n return foo()\r\nend\r\n");CHECK(left && right);
 xide_semantic_source sources[]={{b,left,1,1},{a,right,1,0}};xvalue* proofs[]={proofFor(sources,INT_MAX,1,0),proofFor(sources+1,0,1,1)};CHECK(proofs[0] && proofs[1]);xide_rename_plan_input docs[]={{sources[0],proofs[0]},{sources[1],proofs[1]}};xide_rename_plan_change edits[5];CHECK(changeAt(sources,0,0,edits) && changeAt(sources,0,4,edits+1) && changeAt(sources,0,9,edits+2));const char* p=strstr(right,"foo");CHECK(p && changeAt(sources+1,1,(size_t)(p-right),edits+3));p=strstr(p+3,"foo");CHECK(p && changeAt(sources+1,1,(size_t)(p-right),edits+4));xide_rename_plan_change swap=edits[0];edits[0]=edits[4];edits[4]=swap;
 enum xide_rename_plan_error why;xide_rename_plan* plan=create(docs,edits,"值😀",0,NULL,NULL,&why);CHECK(plan && why==XIDE_RENAME_PLAN_OK && oracle(plan,"值😀"));xide_rename_plan* shorter=create(docs,edits,"x",0,NULL,NULL,&why);CHECK(shorter && oracle(shorter,"x"));xideRenamePlanRelease(shorter);
 CHECK(!create(docs,edits,"return",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_NAME);allocations=0;CHECK(!create(docs,edits,"foo",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_UNCHANGED && !allocations);
 CHECK(!create(docs,edits,"bar",1,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_LIMIT);size_t low=1,high=PLAN_BYTES;while(low<high){size_t mid=low+(high-low)/2;xide_rename_plan* candidate=create(docs,edits,"bar",mid,NULL,NULL,&why);if(candidate){high=mid;xideRenamePlanRelease(candidate);}else{CHECK(why==XIDE_RENAME_PLAN_LIMIT);low=mid+1;}}shorter=create(docs,edits,"bar",low,NULL,NULL,&why);CHECK(shorter);xideRenamePlanRelease(shorter);CHECK(!create(docs,edits,"bar",low-1,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_LIMIT);
 for(size_t i=0;i<2;++i){uint64_t before=blocks();fault=i;allocations=0;shorter=create(docs,edits,"bar",0,NULL,NULL,&why);fault=SIZE_MAX;CHECK(!shorter && why==XIDE_RENAME_PLAN_MEMORY && allocations==i+1 && blocks()==before);before=blocks();cloneFault=i;clones=0;shorter=create(docs,edits,"bar",0,NULL,NULL,&why);cloneFault=SIZE_MAX;CHECK(!shorter && why==XIDE_RENAME_PLAN_MEMORY && clones==i+1 && blocks()==before);}
 cancellation stop={0,SIZE_MAX};shorter=create(docs,edits,"bar",0,cancelPlan,&stop,&why);CHECK(shorter && stop.polls);xideRenamePlanRelease(shorter);size_t polls=stop.polls;for(size_t i=0;i<polls;++i){uint64_t before=blocks();stop=(cancellation){0,i};CHECK(!create(docs,edits,"bar",0,cancelPlan,&stop,&why) && why==XIDE_RENAME_PLAN_CANCELLED && blocks()==before);}
 xide_rename_plan_change bad[5];memcpy(bad,edits,sizeof(bad));bad[1]=bad[0];CHECK(!create(docs,bad,"bar",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_RANGE);memcpy(bad,edits,sizeof(bad));++bad[0].start.character;CHECK(!create(docs,bad,"bar",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_RANGE);
 xide_rename_plan_input duplicate[]={docs[0],docs[0]};CHECK(!create(duplicate,edits,"bar",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_SOURCE);
 CHECK(set(proofs[0],"version",xrtValueInt(INT_MIN)));shorter=create(docs,edits,"bar",0,NULL,NULL,&why);xide_rename_plan_document signedVersion;CHECK(shorter && xideRenamePlanDocumentAt(shorter,1,&signedVersion) && signedVersion.hasVersion && signedVersion.version==INT_MIN);xideRenamePlanRelease(shorter);CHECK(set(proofs[0],"version",xrtValueInt(INT_MAX)));
 xvalue* original=xrtValueDeepClone(proofs[0]);CHECK(original && set(proofs[0],"normalizedSize",xrtValueInt(999)));CHECK(!create(docs,edits,"bar",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_SOURCE);xide_rename_plan_document held;CHECK(xideRenamePlanDocumentAt(plan,1,&held) && xideReferenceDocumentValid(held.proof,held.path));xrtValueRelease(proofs[0]);proofs[0]=original;docs[0].document=original;
 const xvalue* input=get(proofs[1],"input");xvalue* saved=xrtValueDeepClone(input);CHECK(saved && set((xvalue*)input,"rawSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));CHECK(!create(docs,edits,"bar",0,NULL,NULL,&why) && why==XIDE_RENAME_PLAN_SOURCE);CHECK(xideRenamePlanDocumentAt(plan,0,&held) && !xrtValueEqual(get(get(held.proof,"input"),"rawSha256"),get(input,"rawSha256")));CHECK(set(proofs[1],"input",saved));
 stop=(cancellation){0,SIZE_MAX};xvalue* wire=xideRenamePlanWorkspaceEdit(plan,1,cancelPlan,&stop);CHECK(wire && stop.polls);xrtValueRelease(wire);polls=stop.polls;for(size_t i=0;i<polls;++i){uint64_t before=blocks();stop=(cancellation){0,i};CHECK(!xideRenamePlanWorkspaceEdit(plan,1,cancelPlan,&stop) && blocks()==before);}
 for(size_t i=0;i<2;++i){uint64_t before=blocks();cloneFault=i;clones=0;CHECK(!xideRenamePlanWorkspaceEdit(plan,1,NULL,NULL) && clones==i+1 && blocks()==before);cloneFault=SIZE_MAX;}
 reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reader){xideRenamePlanRetain(plan),0};threads[i]=xrtThreadCreate(readPlan,readers+i,0);CHECK(threads[i]);}xrtValueRelease(proofs[0]);xrtValueRelease(proofs[1]);xrtFree(left);xrtFree(right);xrtFree(a);xrtFree(b);for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}CHECK(oracle(plan,"值😀"));xideRenamePlanRelease(plan);return 1;
}
int main(void){if(!run() || !wireLimit())return 1;printf("rename_plan_test passed: %u checks; versioned exact before/after text, independent text-edit oracle, BOM/CRLF/Unicode, offset mapping, proof hashes, overlaps/coordinates, quotas/7MiB wire bound, allocation/clone refusal, cancellation and detached readers\n",checks);return 0;}
