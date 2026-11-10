#include "analysis/frontend_internal.h"
#include "src/lexer/xl6_lexer_api.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_qualified_type_ranges_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int recording;
static size_t allocations[4];
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);
void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);
void* __wrap_xrtMalloc(size_t n){if(recording)++allocations[0];return __real_xrtMalloc(n);}
void* __wrap_xrtCalloc(size_t n,size_t s){if(recording)++allocations[1];return __real_xrtCalloc(n,s);}
void* __wrap_xrtRealloc(void* p,size_t n){if(recording)++allocations[2];return __real_xrtRealloc(p,n);}
void __wrap_xrtFree(void* p){if(recording)++allocations[3];__real_xrtFree(p);}
typedef struct range_case {const char *source,*spelling;size_t names;int accepted;} range_case;
static const range_case cases[]={
 {"func main(...) as int\n auto box=Box::<dep.Marker>()\n return 0\nend\n","dep.Marker",1,1},
 {"func main(...) as int\n auto box=Box::<dep  .  Marker>()\n return 0\nend\n","dep  .  Marker",1,1},
 {"func main(...) as int\n auto box=Box::<one.two.Marker>()\n return 0\nend\n","one.two.Marker",1,1},
 {"func main(...) as int\r\n auto box=Box::<模块.😀类型>()\r\n return 0\r\nend\r\n","模块.😀类型",1,1},
 {"func read(dep.Marker? arg) as dep.Marker?\n return arg\nend\n","dep.Marker",2,1},
 {"func main(...) as int\n auto box=Box::<dep.Marker?>()\n return 0\nend\n","dep.Marker",1,1},
 {"func read(dep.Marker[2] arg)\nend\n","dep.Marker",1,1},
 {"func main(...) as int\n auto box=Box::<dep.Marker<int>>()\n return 0\nend\n","dep.Marker",1,1},
 {"func broken(dep. arg)\nend\n",NULL,0,0},
 {"func main(...) as int\n auto box=Box::<dep.>()\nend\n",NULL,0,0},
 {"func main(...) as int\n auto box=Box::<dep.Marker\n",NULL,0,0},
 {"func main(...) as int\n auto box=Box::<dep.Marker[2]>()\n return 0\nend\n",NULL,0,0},
 {"func main(...) as int\n return 0\nend\n",NULL,0,1},
};
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn;}
static void printRange(XL6_AstRange r)
{printf(" %d %zu %zu %d %d %d %d",r.known,r.startOffset,r.endOffset,r.startLine,r.startColumn,r.endLine,r.endColumn);}
static int names(XL6_AstNode* node,const range_case* c,int strict,int snapshot,size_t ordinal,size_t* count)
{
 if(!node)return 1;
 if(node->kind==XL6_AST_TYPE && node->text && strchr(node->text,'.') && c->spelling){
  size_t start=node->tokenRange.startOffset,end=start+strlen(c->spelling);CHECK(start<=strlen(c->source) && end<=strlen(c->source) && !memcmp(c->source+start,c->spelling,end-start));
  XL6_Lexer lexer;CHECK(xl6LexerOpen(&lexer,c->source));XL6_AstRange first={0},last={0};
  for(;;){XL6_Token token=xl6LexerNext(&lexer);if(token.startOffset==start && !first.known)first=xl6AstRangeFromToken(&token);if(token.endOffset==end)last=xl6AstRangeFromToken(&token);if(token.kind==XL6_TK_EOF || token.kind==XL6_TK_ERROR)break;}
  xl6LexerClose(&lexer);CHECK(first.known && last.known);XL6_AstRange expected=first;xl6AstRangeInclude(&expected,&last);
  if(strict)CHECK(sameRange(node->tokenRange,expected));
  if(snapshot)printf("RANGE %zu %zu %zu %zu %d %d\n",ordinal,*count,node->tokenRange.startOffset,node->tokenRange.endOffset,node->tokenRange.endLine,node->tokenRange.endColumn);
  ++*count;
  /* Normalize only this intended coordinate difference for the complete
     existing exact AST-key oracle. The AST is private to this test. */
  node->tokenRange=first;
 }
 if(snapshot){printf("NODE %zu %d %d %d",ordinal,(int)node->kind,node->line,node->column);printRange(node->range);printRange(node->tokenRange);printf("\n");}
 for(size_t i=0;i<node->childCount;++i)CHECK(names(node->children[i],c,strict,snapshot,ordinal,count));
 return 1;
}
static int rangeRun(int snapshot)
{
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){const range_case* c=cases+i;XL6_ParseResult parsed;xl6ParseResultInit(&parsed);memset(allocations,0,sizeof(allocations));
  recording=1;int accepted=xl6ParseSource(c->source,"/memory/qualified.xl",&parsed);recording=0;CHECK(accepted==c->accepted);
  if(snapshot){printf("CASE %zu %d %zu ALLOC %zu %zu %zu %zu\n",i,accepted,parsed.diags.count,allocations[0],allocations[1],allocations[2],allocations[3]);for(size_t j=0;j<parsed.diags.count;++j)printf("DIAG %d %d %s\n",parsed.diags.items[j].line,parsed.diags.items[j].column,parsed.diags.items[j].message);}
  size_t count=0;CHECK(names(parsed.root,c,!snapshot,snapshot,i,&count));CHECK(count==c->names);
  if(accepted){xstrbuf key={0};unsigned char hash[32];CHECK(xlsFrontendIrAstKey(&key,parsed.root,XLS_IR_KEY_EXACT) && xrtSha256(key.Data,key.Size,hash));if(snapshot){printf("KEY %zu ",i);for(size_t j=0;j<sizeof(hash);++j)printf("%02x",hash[j]);printf("\n");}xrtStrBufFree(&key);}
  xl6ParseResultFree(&parsed);
 }
 return 1;
}
#include "qualified_type_incremental_fixture.h"
int main(int argc,char** argv){int snapshot=argc==2 && !strcmp(argv[1],"--snapshot");if(argc>1 && !snapshot)return 2;if(!rangeRun(snapshot) || (!snapshot && !qualifiedIncremental()))return 1;printf("analysis_qualified_type_ranges_test passed: %u checks; original lexer endpoints, generic/nullable/array/multipart/Unicode source ranges, incremental/fresh index/IR parity and parser allocation/diagnostic/exact-AST-key snapshot\n",checks);return 0;}
