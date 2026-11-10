#include "services/semantic_symbols.h"
#include "analysis/check.h"
#include "analysis/text.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "declaration_symbols_source.h"

/* Actual implementation, with allocation/array refusal only at its service
 * boundary. The native checker and its immutable validation certificate are
 * built normally before injection. Detect any repeated full-table allocation. */
static size_t faultAt,operations,crtAllocations,pointConversions;static int tracking,failedOptional;
static int operation(int optional){if(!tracking || ++operations!=faultAt)return 1;failedOptional=optional;return 0;}
static void* symbolMalloc(size_t n){return operation(0) ? xrtMalloc(n) : NULL;}
static void* symbolCalloc(size_t n,size_t size){return operation(0) ? xrtCalloc(n,size) : NULL;}
static char* symbolDup(const char* text){return operation(0) ? xrtStrDup(text) : NULL;}
static bool symbolArrayInit(xarray* a,size_t size){return operation(size==sizeof(uint32_t)) && xrtArrayInit(a,size);}
static bool symbolArrayResize(xarray* a,size_t count){return operation(a->ItemSize==sizeof(uint32_t)) && xrtArrayResize(a,count);}
static bool symbolArrayReserve(xarray* a,size_t count){return operation(0) && xrtArrayReserve(a,count);}
static bool symbolArrayPush(xarray* a,const void* value){return operation(0) && xrtArrayPush(a,value);}
static bool symbolArraySort(xarray* a,xarraycompare compare){return operation(0) && xrtArraySort(a,compare);}
static int observePosition(const xls_text* text,size_t offset,xls_position* position){if(tracking)++pointConversions;return xlsTextPosition(text,offset,position);}
static void* observeMalloc(size_t n){if(tracking)++crtAllocations;return malloc(n);}
static void* observeCalloc(size_t n,size_t size){if(tracking)++crtAllocations;return calloc(n,size);}
#define xrtMalloc symbolMalloc
#define xrtCalloc symbolCalloc
#define xrtStrDup symbolDup
#define xrtArrayInit symbolArrayInit
#define xrtArrayResize symbolArrayResize
#define xrtArrayReserve symbolArrayReserve
#define xrtArrayPush symbolArrayPush
#define xrtArraySort symbolArraySort
#define malloc observeMalloc
#define calloc observeCalloc
#define xlsTextPosition observePosition
#include "../src/services/semantic_definition.c"
#undef xrtMalloc
#undef xrtCalloc
#undef xrtStrDup
#undef xrtArrayInit
#undef xrtArrayResize
#undef xrtArrayReserve
#undef xrtArrayPush
#undef xrtArraySort
#undef malloc
#undef calloc
#undef xlsTextPosition

static size_t checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"declaration_symbols_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t liveBlocks(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static int valueEqual(xvalue* a,xvalue* b){int same=a && b && xrtValueEqual(a,b);xrtValueRelease(a);xrtValueRelease(b);return same;}
static int cancelledAt(void* user){size_t* state=user;++state[0];if(state[0]==state[1])state[2]=1;return state[2]!=0;}
static int sameFact(const xls_index_fact* a,const xls_index_fact* b)
{
 return a->id==b->id && a->parent==b->parent && a->source==b->source && a->selectionSource==b->selectionSource &&
  a->declaredSymbol==b->declaredSymbol && a->symbol==b->symbol && a->targetSymbol==b->targetSymbol &&
  a->compoundOperatorSymbol==b->compoundOperatorSymbol && a->declaredType==b->declaredType && a->referencedType==b->referencedType &&
  a->typeId==b->typeId && a->typeObjectId==b->typeObjectId && a->typeAbi==b->typeAbi && a->typeObjectAbi==b->typeObjectAbi && a->typeOrigin==b->typeOrigin &&
  !strcmp(a->kind,b->kind) && ((!a->canonical && !b->canonical) || (a->canonical && b->canonical && !strcmp(a->canonical,b->canonical))) &&
  sameCompilerRange(a->range,b->range) && sameCompilerRange(a->tokenRange,b->tokenRange) && sameCompilerRange(a->selectionRange,b->selectionRange);
}
static int expectedSymbols(xide_semantic_symbols* symbols,const char* original,const char* path)
{
 CHECK(symbols && xideSemanticSymbolsCount(symbols)==34 && xideSemanticSymbolsBytes(symbols)<128u*1024u);
 unsigned kinds[27]={0};size_t roots=0;int previous=-1,left=-1,right=-1;
 for(size_t i=0;i<xideSemanticSymbolsCount(symbols);++i){
  xide_semantic_symbol_view item;CHECK(xideSemanticSymbolAt(symbols,i,&item));CHECK(!strcmp(item.path,path) && item.name && !strstr(item.name,"hidden_") && item.kind>0 && item.kind<=26);++kinds[item.kind];
  int offset=0,end=0;CHECK(xideLspPositionToOffset(original,item.selectionStart.line,item.selectionStart.character,&offset));CHECK(xideLspPositionToOffset(original,item.selectionEnd.line,item.selectionEnd.character,&end));
  CHECK(offset>=previous && end>offset && (size_t)(end-offset)==strlen(item.name) && !memcmp(original+offset,item.name,(size_t)(end-offset)));previous=offset;
  if(item.parent==SIZE_MAX)++roots;else{ xide_semantic_symbol_view parent;CHECK(item.parent<i && xideSemanticSymbolAt(symbols,item.parent,&parent));CHECK(comparePoint((point){parent.start.line,parent.start.character},(point){item.start.line,item.start.character})<=0 && comparePoint((point){item.end.line,item.end.character},(point){parent.end.line,parent.end.character})<=0);}
  if(!strcmp(item.name,"left"))left=(int)i;
  if(!strcmp(item.name,"right"))right=(int)i;
 }
 CHECK(roots==13 && kinds[5]==2 && kinds[11]==1 && kinds[10]==1 && kinds[22]==2 && kinds[23]==1 && kinds[26]==3);
 CHECK(kinds[13]==5 && kinds[14]==1 && kinds[8]==5 && kinds[9]==2 && kinds[7]==4 && kinds[6]==5 && kinds[12]==2);
 CHECK(left>=0 && right>left);xide_semantic_symbol_view l,r;CHECK(xideSemanticSymbolAt(symbols,(size_t)left,&l) && xideSemanticSymbolAt(symbols,(size_t)right,&r));
 CHECK(l.start.line==r.start.line && l.start.character==r.start.character && l.end.line==r.end.line && l.end.character==r.end.character && strstr(l.detail,"auto left,right"));
 xvalue* rich=xideSemanticSymbolsDocument(symbols,path,NULL);CHECK(rich && xrtValueCount(rich)==13);xrtValueRelease(rich);
 char* uri=xideLspPathToUri(path,NULL);CHECK(uri);xvalue* flat=xideSemanticSymbolsDocument(symbols,path,uri);CHECK(flat && xrtValueCount(flat)==34);xrtValueRelease(flat);free(uri);
 CHECK(!xideSemanticSymbolAt(symbols,34,&l) && !xideSemanticSymbolAt(NULL,0,&l) && !xideSemanticSymbolAt(symbols,0,NULL));
 return 1;
}
static int minified(void)
{
 char* path=xrtPathAbs("declaration-long-line.xl");xstrbuf original={0};CHECK(path && xrtStrBufAppend(&original,xrtStrView("\xef\xbb\xbf/* 中文😀 */ ")));
 for(unsigned i=0;i<512;++i){char text[96];snprintf(text,sizeof(text),"func f%u() as int;return %u;end;",i,i);CHECK(xrtStrBufAppend(&original,xrtStrView(text)));}
 CHECK(xrtStrBufAppend(&original,xrtStrView("func main(...) as int;return 0;end")));
 xls_semantic_input input={path,original.Data,original.Size};char detail[512]={0};
 xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,detail,sizeof(detail));CHECK(check && xlsSemanticCheckInfo(check).accepted);
 xls_input_source source;CHECK(xlsSemanticCheckSource(check,0,&source));xide_semantic_source sources[]={{source.path,source.text,source.kind,1}};xide_model_error error={0};
 xide_navigation_snapshot* snapshot=xideSemanticNavigationCapture(check,sources,1,&error);CHECK(snapshot && xideSemanticNavigationPreparePoints(snapshot,&error)==1);
 xide_symbol_work work={0};tracking=1;operations=crtAllocations=pointConversions=faultAt=0;
 xide_semantic_symbols* symbols=xideSemanticSymbolsFromNavigation(snapshot,NULL,NULL,&work,&error);tracking=0;
 CHECK(symbols && work.reusedValidation && !work.fullValidationFallback && !crtAllocations && !pointConversions && work.declarations==513 && work.coordinateEndpoints==513*4);
 CHECK(work.denseCoordinates && work.constructionAllowanceBytes<=64u*1024u*1024u);
 for(size_t i=0;i<513;++i){xide_semantic_symbol_view item;CHECK(xideSemanticSymbolAt(symbols,i,&item) && item.kind==12 && item.parent==SIZE_MAX && item.start.line==0 && item.end.line==0);
  int first=0,last=0;CHECK(xideLspPositionToOffset(original.Data,item.selectionStart.line,item.selectionStart.character,&first) && xideLspPositionToOffset(original.Data,item.selectionEnd.line,item.selectionEnd.character,&last));
  CHECK((size_t)(last-first)==strlen(item.name) && !memcmp(original.Data+first,item.name,(size_t)(last-first)));}
 printf("Minified declarations: %zu; batch endpoints: %zu; per-offset line scans: %zu; owned bytes: %zu.\n",work.declarations,work.coordinateEndpoints,pointConversions,work.ownedBytes);
 xideSemanticSymbolsDestroy(symbols);xideSemanticNavigationDestroy(snapshot);xlsSemanticCheckDestroy(check);xrtStrBufFree(&original);xrtFree(path);return 1;
}
static int run(void)
{
 char* path=xrtPathAbs("declaration-中文😀.xl");xstrbuf original={0};CHECK(path && xrtStrBufAppend(&original,xrtStrView("\xef\xbb\xbf")) && xrtStrBufAppend(&original,xrtStrView(declarationSymbolsBody)));
 xls_semantic_input input={path,original.Data,original.Size};char errorText[512]={0};
 xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,errorText,sizeof(errorText));CHECK(check);
 if(!xlsSemanticCheckInfo(check).accepted){for(size_t i=0;i<xlsSemanticCheckInfo(check).diagnostics;++i){xls_check_diagnostic d;if(xlsSemanticCheckDiagnostic(check,i,&d))fprintf(stderr,"%d:%d %s\n",d.line,d.column,d.message);}}
 CHECK(xlsSemanticCheckInfo(check).accepted);
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);CHECK(index);
 size_t fieldsWithIds=0,multi=0,scopes=0;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){
  xls_declaration_fact f={0};xls_index_fact raw={0};CHECK(xlsSemanticIndexDeclarationFact(index,i,&f) && xlsSemanticIndexFact(index,i,&raw) && sameFact(&f.fact,&raw));
  if(f.fact.source!=1)continue;
  if(f.shape.kind==XLS_DECL_FIELD){CHECK(f.fact.declaredSymbol && f.fact.declaredSymbol==f.fact.symbol && f.fact.selectionSource==f.nameSource && f.fact.selectionRange.known && f.fact.selectionRange.startOffset==f.nameRange.startOffset && f.fact.selectionRange.endOffset==f.nameRange.endOffset);++fieldsWithIds;}
  if(f.declarationNode!=f.fact.id){CHECK(f.shape.kind==XLS_DECL_VARIABLE && !strcmp(f.fact.kind,"name"));++multi;}
  if(f.shape.callable)++scopes;
  if(f.nameRange.known){xls_index_fact token;CHECK(symbolNativeName(index,&f,&token));}
 }
 CHECK(fieldsWithIds==5 && multi==2 && scopes>=13);
 xls_declaration_fact invalid={0};CHECK(!xlsSemanticIndexDeclarationFact(index,xlsSemanticIndexCount(index),&invalid) && !invalid.fact.id && !xlsSemanticIndexDeclarationFact(NULL,0,&invalid) && !xlsSemanticIndexDeclarationFact(index,0,NULL));
 xls_inputs inputs;CHECK(xlsSemanticCheckInputs(check,&inputs) && inputs.sources==1);xls_input_source source;CHECK(xlsSemanticCheckSource(check,0,&source));
 xide_semantic_source sources[]={{source.path,source.text,source.kind,1}};xide_model_error error={0};
 xls_projection_store* bank=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(bank);
 xide_navigation_snapshot* snapshot=xideSemanticNavigationCapture(check,sources,1,&error);CHECK(snapshot);xideSemanticNavigationBind(snapshot,bank);CHECK(xideSemanticNavigationPreparePoints(snapshot,&error)==1);
 xide_navigation_snapshot* retained=xideSemanticNavigationRetain(snapshot);xideSemanticNavigationDestroy(snapshot);snapshot=retained;
 xide_symbol_work work={0};
 xide_semantic_symbols* warm=xideSemanticSymbolsFromNavigation(snapshot,NULL,NULL,&work,&error);CHECK(warm && expectedSymbols(warm,original.Data,path));xideSemanticSymbolsDestroy(warm);
 uint64_t before=liveBlocks();tracking=1;operations=crtAllocations=pointConversions=faultAt=0;
 xide_semantic_symbols* symbols=xideSemanticSymbolsFromNavigation(snapshot,NULL,NULL,&work,&error);tracking=0;size_t totalOperations=operations;
 CHECK(symbols && work.reusedValidation && !work.fullValidationFallback && !crtAllocations && !pointConversions && work.facts==xlsSemanticIndexCount(index) && work.declarations==34 && work.ownedBytes==xideSemanticSymbolsBytes(symbols));
 CHECK(work.coordinateEndpoints==34*4 && work.denseCoordinates && work.constructionAllowanceBytes<=64u*1024u*1024u);
 CHECK(expectedSymbols(symbols,original.Data,path));xideSemanticSymbolsDestroy(symbols);CHECK(liveBlocks()==before);
 for(size_t fail=1;fail<=totalOperations;++fail){
  tracking=1;faultAt=fail;operations=crtAllocations=pointConversions=0;failedOptional=0;symbols=xideSemanticSymbolsFromNavigation(snapshot,NULL,NULL,&work,&error);tracking=0;
  CHECK(operations>=fail && !crtAllocations && !pointConversions && xideSemanticNavigationPointsReady(snapshot));
  if(failedOptional){CHECK(symbols && !error.code && work.reusedValidation && !work.denseCoordinates && expectedSymbols(symbols,original.Data,path));xideSemanticSymbolsDestroy(symbols);}
  else CHECK(!symbols && error.code==XIDE_MODEL_IO);
  CHECK(liveBlocks()==before);
 }
 printf("declaration refusal points: %zu; verified point view remains unchanged; no repeated CRT node/type/reference table allocations.\n",totalOperations);
 size_t state[3]={0,SIZE_MAX,0};symbols=xideSemanticSymbolsFromNavigation(snapshot,cancelledAt,state,&work,&error);CHECK(symbols);xideSemanticSymbolsDestroy(symbols);size_t callbacks=state[0];
 for(size_t stop=1;stop<=callbacks;++stop){size_t request[3]={0,stop,0};CHECK(!xideSemanticSymbolsFromNavigation(snapshot,cancelledAt,request,&work,&error) && request[2] && liveBlocks()==before);}
 symbols=xideSemanticSymbolsFromNavigation(snapshot,NULL,NULL,&work,&error);CHECK(symbols && expectedSymbols(symbols,original.Data,path));
 xide_semantic_symbols* fallback=xideSemanticSymbolsCreateTyped(index,sources,1,NULL,NULL,&work,&error);CHECK(fallback && work.fullValidationFallback && !work.reusedValidation && valueEqual(xideSemanticSymbolsDocument(symbols,path,NULL),xideSemanticSymbolsDocument(fallback,path,NULL)));xideSemanticSymbolsDestroy(fallback);
 const char* raw=xlsSemanticCheckIndex(check);CHECK(raw);xvalue* json=xrtJsonParse(xrtStrView(raw));CHECK(json);fallback=xideSemanticSymbolsCreate(json,sources,1,NULL,NULL,&work,&error);CHECK(fallback && valueEqual(xideSemanticSymbolsDocument(symbols,path,NULL),xideSemanticSymbolsDocument(fallback,path,NULL)));xideSemanticSymbolsDestroy(fallback);xrtValueRelease(json);
 /* Owned declarations outlive their entire AST/source/validation/cache cohort. */
 xlsProjectionStoreDestroy(bank);xideSemanticNavigationDestroy(snapshot);xlsSemanticCheckDestroy(check);
 CHECK(expectedSymbols(symbols,original.Data,path));xideSemanticSymbolsDestroy(symbols);
 xrtStrBufFree(&original);xrtFree(path);return 1;
}
int main(void)
{if(!run() || !minified())return 1;printf("declaration_symbols_test passed: %zu checks; source roles, stable raw facts, typed/JSON/proof parity, callable locals, sorted parents, refusals, cancellation, shared certificate and detached ownership.\n",checks);return 0;}
