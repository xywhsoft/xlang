#define main originalDispatchMain
#include "semantic_dispatch_family_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_reference_merge_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static char* unicodeRead(const char* text)
{
 xstrbuf out={0};for(const char* p=text;*p;){if(!strncmp(p,"read",4)){if(!xrtStrBufAppend(&out,xrtStrView("读取😀")))goto failed;p+=4;}else{if(!xrtStrBufAppend(&out,(xstrview){p,1}))goto failed;++p;}}return out.Data;
failed:xrtStrBufFree(&out);return NULL;
}
static int mergeRun(int unicode)
{
 const char* name=unicode ? "读取😀" : "read";size_t nameBytes=strlen(name);
 char* dir=xrtPathAbs("build/reference-merge/源码😀");CHECK(dir && xrtDirCreateAll(dir));
 char *a=xrtPathJoin(dir,"first.xl"),*b=xrtPathJoin(dir,"second.xl"),*dep=xrtPathJoin(dir,"dep.xl");CHECK(a && b && dep);
 const char *lib="\xef\xbb\xbf" "export protocol Reader\r\n func read() as int\r\nend\r\nexport class Base\r\n virtual func read() as int\r\n return 1\r\n end\r\nend\r\n";
 const char *first="import dep\nclass Derived extends dep.Base\n override func read() as int\n return 2\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Derived())\nend\n";
 const char *second="import dep\nfunc extra() as int\n return 0\nend\nclass Other extends dep.Base\n override func read() as int\n return 3\n end\nend\nclass Wrong\n func read(int value) as int\n return value+4\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Other())\nend\n";
 char* translated[3]={0};if(unicode){translated[0]=unicodeRead(lib);translated[1]=unicodeRead(first);translated[2]=unicodeRead(second);CHECK(translated[0] && translated[1] && translated[2]);lib=translated[0];first=translated[1];second=translated[2];}
 xls_semantic_input inputs[]={{a,first,strlen(first)},{dep,lib,strlen(lib)}};family_owner owners[2]={0};CHECK(captureFamily(owners,a,inputs,2));inputs[0]=(xls_semantic_input){b,second,strlen(second)};CHECK(captureFamily(owners+1,b,inputs,2));
 enum xide_reference_family_error why;xide_reference_graph* graphs[2];
 for(size_t i=0;i<2;++i){graphs[i]=xideSemanticReferenceGraphCreate(owners[i].cache,owners[i].check,owners[i].sources,owners[i].count,0,NULL,NULL,&why);CHECK(graphs[i]);}
 xide_reference_key* key=familyKeyAt(owners,a,first,name);CHECK(key);
 char wrong[96];snprintf(wrong,sizeof(wrong),"%s(int value)",name);xide_reference_key* unrelated=familyKeyAt(owners+1,b,second,wrong);CHECK(unrelated);xide_reference_family* unrelatedFamily=xideSemanticReferenceGraphFamily(graphs[1],unrelated,0,NULL,NULL,&why);xide_reference_family_info ui;CHECK(unrelatedFamily && xideSemanticReferenceFamilyInfo(unrelatedFamily,&ui) && ui.members==1 && !ui.kinds);xideSemanticReferenceFamilyRelease(unrelatedFamily);xideSemanticReferenceKeyRelease(unrelated);
 xide_reference_family* local=xideSemanticReferenceGraphFamily(graphs[0],key,0,NULL,NULL,&why);xide_reference_family_info info;CHECK(local && xideSemanticReferenceFamilyInfo(local,&info) && info.members==3);xideSemanticReferenceFamilyRelease(local);
 xide_reference_family* f=xideSemanticReferenceGraphsFamily(graphs,2,key,0,NULL,NULL,&why);
 if(!f)fprintf(stderr,"merge error=%d\n",(int)why);
 CHECK(f && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticReferenceFamilyInfo(f,&info) && info.members==4 && info.sources==3 && info.kinds==3);
 for(size_t i=0;i<info.members;++i){xide_semantic_source s;size_t start,end;CHECK(xideSemanticReferenceFamilyMember(f,i,&s,&start,&end) && end-start==nameBytes && !memcmp(s.text+start,name,nameBytes));}
 xide_semantic_source empty={.path="bad"};size_t start=9,end=9;CHECK(!xideSemanticReferenceFamilyMember(f,info.members,&empty,&start,&end) && !empty.path && !start && !end);
 xide_reference_graph* reversed[]={graphs[1],graphs[0],graphs[1]};xide_reference_family* other=xideSemanticReferenceGraphsFamily(reversed,3,key,0,NULL,NULL,&why);xide_reference_family_info ri;CHECK(other && xideSemanticReferenceFamilyInfo(other,&ri) && ri.members==info.members && ri.sources==info.sources && ri.kinds==info.kinds && ri.relations>info.relations);
 for(size_t i=0;i<info.members;++i){xide_semantic_source x,y;size_t xf,xl,yf,yl;CHECK(xideSemanticReferenceFamilyMember(f,i,&x,&xf,&xl) && xideSemanticReferenceFamilyMember(other,i,&y,&yf,&yl) && !strcmp(x.path,y.path) && xf==yf && xl==yl);}
 xideSemanticReferenceFamilyRelease(other);
 CHECK(!xideSemanticReferenceGraphsFamily(NULL,2,key,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);
 CHECK(!xideSemanticReferenceGraphsFamily(graphs,2,key,1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 size_t low=1,high=128u*1024u*1024u;while(low<high){size_t mid=low+(high-low)/2;other=xideSemanticReferenceGraphsFamily(graphs,2,key,mid,NULL,NULL,&why);if(other){high=mid;xideSemanticReferenceFamilyRelease(other);}else{CHECK(why==XIDE_REFERENCE_FAMILY_LIMIT);low=mid+1;}}
 other=xideSemanticReferenceGraphsFamily(graphs,2,key,low,NULL,NULL,&why);CHECK(other);xideSemanticReferenceFamilyRelease(other);CHECK(!xideSemanticReferenceGraphsFamily(graphs,2,key,low-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 for(size_t i=0;i<2;++i){uint64_t before=blocks();familyFaultAt=i;familyAllocations=0;other=xideSemanticReferenceGraphsFamily(graphs,2,key,0,NULL,NULL,&why);familyFaultAt=SIZE_MAX;CHECK(!other && why==XIDE_REFERENCE_FAMILY_MEMORY && familyAllocations==i+1 && blocks()==before);}
 cancellation stop={0,SIZE_MAX};other=xideSemanticReferenceGraphsFamily(graphs,2,key,0,cancelFamily,&stop,&why);CHECK(other && stop.polls>8);xideSemanticReferenceFamilyRelease(other);size_t polls=stop.polls;
 for(size_t i=0;i<polls;++i){uint64_t before=blocks();stop=(cancellation){0,i};other=xideSemanticReferenceGraphsFamily(graphs,2,key,0,cancelFamily,&stop,&why);CHECK(!other && why==XIDE_REFERENCE_FAMILY_CANCELLED && blocks()==before);}
 /* A connected unsupported provider cannot disappear during cross-owner union. */
 graph_component* component=NULL;for(size_t i=0;i<graphs[0]->count;++i){graph_entry* e=graphs[0]->entries+i;if(e->start==key->start && e->end==key->end && !strcmp(graphs[0]->shard->source[e->source-1].path,key->path)){component=graphs[0]->groups+e->component;break;}}
 CHECK(component);unsigned saved=component->unsupported;component->unsupported=1;CHECK(!xideSemanticReferenceGraphsFamily(graphs,2,key,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_UNSUPPORTED);component->unsupported=saved;
 /* Same path/length and same declaration intervals, different complete input. */
 char* changed=xrtStrDup(first);CHECK(changed);char* value=strstr(changed,"return 2");CHECK(value);value[7]='5';family_owner different={0};inputs[0]=(xls_semantic_input){a,changed,strlen(changed)};CHECK(captureFamily(&different,a,inputs,2));xide_reference_graph* bad=xideSemanticReferenceGraphCreate(different.cache,different.check,different.sources,different.count,0,NULL,NULL,&why);CHECK(bad);xide_reference_graph* mismatch[]={graphs[0],bad};CHECK(!xideSemanticReferenceGraphsFamily(mismatch,2,key,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);CHECK(!xideSemanticReferenceGraphsFamily(&bad,1,key,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);xideSemanticReferenceGraphRelease(bad);clearFamily(&different);xrtFree(changed);
 xide_reference_key* lease=xideSemanticReferenceFamilyKey(f,0);CHECK(lease);family_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(family_reader){xideSemanticReferenceFamilyRetain(f),name,4,0};threads[i]=xrtThreadCreate(readFamily,readers+i,0);CHECK(threads[i]);}
 xideSemanticReferenceFamilyRelease(f);xideSemanticReferenceKeyRelease(key);for(size_t i=0;i<2;++i){xideSemanticReferenceGraphRelease(graphs[i]);clearFamily(owners+i);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xstrview spelling;int units;CHECK(xideSemanticReferenceKeyIdentifier(lease,&spelling,&units) && spelling.Size==nameBytes && !memcmp(spelling.Data,name,nameBytes) && units==4);xideSemanticReferenceKeyRelease(lease);
 for(size_t i=0;i<3;++i)xrtFree(translated[i]);
 xrtFree(a);xrtFree(b);xrtFree(dep);xrtFree(dir);return 1;
}
int main(void){if(!mergeRun(0) || !mergeRun(1))return 1;printf("semantic_reference_merge_test passed: %u checks; cross-owner native component closure, shared-source dedup, exact source guards, quota/refusal/cancellation, Unicode/BOM/CRLF and detached four-reader lifetime\n",checks);return 0;}
