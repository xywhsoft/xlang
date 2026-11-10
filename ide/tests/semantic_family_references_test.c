#define main originalDispatchMain
#include "semantic_dispatch_family_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_family_references_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int referenceCompare(const void* a,const void* b)
{const xide_semantic_reference *x=a,*y=b;int n=strcmp(x->path,y->path);if(!n)n=(x->start.offset>y->start.offset)-(x->start.offset<y->start.offset);if(!n)n=(x->end.offset>y->end.offset)-(x->end.offset<y->end.offset);return n ? n : x->declaration-y->declaration;}
static int referenceOracle(family_owner* o,xide_reference_family* family,xide_family_references* refs,int include)
{
 xarray expected={0},actual={0};CHECK(xrtArrayInit(&expected,sizeof(xide_semantic_reference)) && xrtArrayInit(&actual,sizeof(xide_semantic_reference)));xide_reference_family_info fi;xide_family_reference_info ri;CHECK(xideSemanticReferenceFamilyInfo(family,&fi) && xideSemanticFamilyReferencesInfo(refs,&ri));
 for(size_t i=0;i<fi.members;++i){xide_reference_key* key=xideSemanticReferenceFamilyKey(family,i);xide_reference_identifier_view view;CHECK(key && xideSemanticReferenceShardIdentifiers(o->shard,key,include,&view)==XIDE_REFERENCE_MATCH);for(size_t j=0;j<view.references.count;++j){xide_semantic_reference at;CHECK(xideSemanticReferenceShardIdentifierAt(&view,j,&at) && xrtArrayPush(&expected,&at));}xideSemanticReferenceKeyRelease(key);}
 for(size_t i=0;i<ri.locations;++i){xide_semantic_reference at;CHECK(xideSemanticFamilyReferenceAt(refs,i,&at) && xrtArrayPush(&actual,&at));}
 CHECK(expected.Count==actual.Count && actual.Count>=fi.members-(include?0:fi.members));qsort(expected.Data,expected.Count,sizeof(xide_semantic_reference),referenceCompare);qsort(actual.Data,actual.Count,sizeof(xide_semantic_reference),referenceCompare);
 for(size_t i=0;i<actual.Count;++i){const xide_semantic_reference *x=(xide_semantic_reference*)actual.Data+i,*y=(xide_semantic_reference*)expected.Data+i;CHECK(!referenceCompare(x,y) && x->start.line==y->start.line && x->start.character==y->start.character && x->end.line==y->end.line && x->end.character==y->end.character);}
 xrtArrayUnit(&expected);xrtArrayUnit(&actual);return 1;
}
typedef struct reference_reader {xide_family_references* refs;size_t count,bytes;int okay;} reference_reader;
static int32 referenceRead(void* data)
{reference_reader* r=data;r->okay=1;for(size_t i=0;i<120;++i){xide_semantic_reference at;if(!xideSemanticFamilyReferenceAt(r->refs,i%r->count,&at) || at.end.offset-at.start.offset!=(int)r->bytes || !at.path)r->okay=0;}return 0;}
static int runReferenceProfile(const char* raw,const char* name,size_t expectedMembers)
{
 char* path=xrtPathAbs("build/family-references/源码😀.xl");CHECK(path);family_owner o={0};xls_semantic_input input={path,raw,strlen(raw)};CHECK(captureFamily(&o,path,&input,1));xide_reference_key* key=familyKeyAt(&o,path,raw,name);CHECK(key);enum xide_reference_family_error why;xide_reference_family* family=makeFamily(&o,key,0,NULL,NULL,&why);CHECK(family);
 xide_family_references* refs=xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why);xide_family_reference_info info;CHECK(refs && why==XIDE_REFERENCE_FAMILY_OK && xideSemanticFamilyReferencesInfo(refs,&info));CHECK(info.sourceComparisons==1 && info.sourceBytesCompared==family->source[0].size && info.referenceRowsVisited==2*o.shard->count && info.locations>expectedMembers);CHECK(referenceOracle(&o,family,refs,1));
 xide_family_references* other=xideSemanticReferenceShardFamily(o.shard,family,0,0,NULL,NULL,&why);xide_family_reference_info hidden;CHECK(other && xideSemanticFamilyReferencesInfo(other,&hidden) && hidden.locations+expectedMembers==info.locations && referenceOracle(&o,family,other,0));xideSemanticFamilyReferencesRelease(other);
 char* absent=xrtPathAbs("build/family-references/other.xl");const char* plain="func main(...) as int\n return 0\nend\n";family_owner emptyOwner={0};xls_semantic_input emptyInput={absent,plain,strlen(plain)};CHECK(absent && captureFamily(&emptyOwner,absent,&emptyInput,1));other=xideSemanticReferenceShardFamily(emptyOwner.shard,family,1,0,NULL,NULL,&why);CHECK(other && xideSemanticFamilyReferencesInfo(other,&hidden) && !hidden.locations && !hidden.sourceComparisons && !hidden.referenceRowsVisited);xideSemanticFamilyReferencesRelease(other);clearFamily(&emptyOwner);xrtFree(absent);
 xide_semantic_reference empty={.path="bad"};CHECK(!xideSemanticFamilyReferenceAt(refs,info.locations,&empty) && !empty.path);xide_family_reference_info missing={.locations=9};CHECK(!xideSemanticFamilyReferencesInfo(NULL,&missing) && !missing.locations);
 CHECK(!xideSemanticReferenceShardFamily(o.shard,family,2,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);
 CHECK(!xideSemanticReferenceShardFamily(o.shard,family,1,1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 size_t low=1,high=XIDE_REFERENCE_BYTES;while(low<high){size_t mid=low+(high-low)/2;other=xideSemanticReferenceShardFamily(o.shard,family,1,mid,NULL,NULL,&why);if(other){high=mid;xideSemanticFamilyReferencesRelease(other);}else{CHECK(why==XIDE_REFERENCE_FAMILY_LIMIT);low=mid+1;}}
 other=xideSemanticReferenceShardFamily(o.shard,family,1,low,NULL,NULL,&why);CHECK(other);xideSemanticFamilyReferencesRelease(other);CHECK(!xideSemanticReferenceShardFamily(o.shard,family,1,low-1,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_LIMIT);
 for(size_t i=0;i<2;++i){uint64_t before=blocks();familyFaultAt=i;familyAllocations=0;other=xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why);familyFaultAt=SIZE_MAX;CHECK(!other && why==XIDE_REFERENCE_FAMILY_MEMORY && familyAllocations==i+1 && blocks()==before);}
 cancellation stop={0,SIZE_MAX};other=xideSemanticReferenceShardFamily(o.shard,family,1,0,cancelFamily,&stop,&why);CHECK(other && stop.polls);xideSemanticFamilyReferencesRelease(other);size_t polls=stop.polls;
 for(size_t i=0;i<polls;++i){uint64_t before=blocks();stop=(cancellation){0,i};other=xideSemanticReferenceShardFamily(o.shard,family,1,0,cancelFamily,&stop,&why);CHECK(!other && why==XIDE_REFERENCE_FAMILY_CANCELLED && blocks()==before);}
 char saved=family->source[0].text[0];family->source[0].text[0]^=1;CHECK(!xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);family->source[0].text[0]=saved;
 int prefix=family->source[0].prefix;family->source[0].prefix^=1;CHECK(!xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_INVALID);family->source[0].prefix=prefix;
 saved=family->source[0].path[0];family->source[0].path[0]=saved>='a' && saved<='z' ? (char)(saved-32) : (char)(saved+32);other=xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why);family->source[0].path[0]=saved;CHECK(other && referenceOracle(&o,family,other,1));xideSemanticFamilyReferencesRelease(other);
 /* Reject the complete capture when one already-bound occurrence cannot be
  * projected; do not quietly omit it. Restore the real immutable data after. */
 size_t selected=SIZE_MAX;for(size_t i=0;i<o.shard->count;++i){reference_row* row=&o.shard->items[i].row;if(row->targetStart==key->start && row->targetEnd==key->end && !strcmp(o.shard->source[row->targetSource-1].path,key->path)){selected=i;break;}}CHECK(selected!=SIZE_MAX);int savedColumn=o.shard->items[selected].end.character;o.shard->items[selected].end.character=0;CHECK(!xideSemanticReferenceShardFamily(o.shard,family,1,0,NULL,NULL,&why) && why==XIDE_REFERENCE_FAMILY_UNSUPPORTED);o.shard->items[selected].end.character=savedColumn;
 reference_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reference_reader){refs,info.locations,strlen(name),0};threads[i]=xrtThreadCreate(referenceRead,readers+i,0);CHECK(threads[i]);}
 xideSemanticReferenceFamilyRelease(family);xideSemanticReferenceKeyRelease(key);clearFamily(&o);for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xideSemanticFamilyReferencesRelease(refs);xrtFree(path);return 1;
}
int main(void)
{
 if(!runReferenceProfile(dispatchFamilyText,"read",5))return 1;
 xstrbuf text={0};if(!xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf")))return 1;for(const char* p=dispatchFamilyText;*p;){if(!strncmp(p,"read",4)){if(!xrtStrBufAppend(&text,xrtStrView("读取😀")))return 1;p+=4;}else if(*p=='\n'){if(!xrtStrBufAppend(&text,xrtStrView("\r\n")))return 1;++p;}else{if(!xrtStrBufAppend(&text,(xstrview){p,1}))return 1;++p;}}
 int okay=runReferenceProfile(text.Data,"读取😀",5);xrtStrBufFree(&text);if(!okay)return 1;
 const char* indexed="class Indexed\n int data\n get item() as int\n return self.data\n end\n get item(int index) as int\n return self.data+index\n end\n let item(int value)\n self.data=value\n end\n let item(int index,int value)\n self.data=value+index\n end\nend\nfunc main(...) as int\n auto box=Indexed()\n int at=2\n int value=3\n box.item=1\n box.item[at]=value\n box.item[at]+=value\n return box.item+box.item[at]\nend\n";
 if(!runReferenceProfile(indexed,"item",4))return 1;
 printf("semantic_family_references_test passed: %u checks; all-member native identifier parity, once-per-source proof, declaration filtering, indexed/compound properties, empty fast path, Unicode/BOM/CRLF, actual allocation refusal, cancellation and detached four-reader lifetime\n",checks);return 0;
}
