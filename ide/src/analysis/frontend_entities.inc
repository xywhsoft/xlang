/* Native typed-index boundary only. Original collector/checker/IDs unchanged.
 * Keys describe verified declaration syntax and lexical owner,never source
 * offsets,compiler IDs,pointers or a hash-only match. References are bound ONLY
 * through the actual current compiler SymbolId/TypeId maps. */
#include "analysis/index_entities.h"
#include "analysis/frontend_cancel.h"
#include "analysis/module_store.h"
#include "analysis/frontend_internal.h"
#define XLS_ENTITY_KEY_BYTES 65536u
#define XLS_ENTITY_VIEW_BYTES (16u*1024u*1024u)
typedef struct xls_entity_binding {uint32_t native;uint64_t stable;int ambiguous;} xls_entity_binding;
struct xls_entity_view {xatomic32 references;xls_entity_store* store;const xls_entity_domain* domain;xls_module_graph* modules;xarray symbols,types,reverseSymbols,versions;size_t ambiguous,bytes,versionDeclined,versionSymbols,versionTypes;};
typedef struct xls_entity_scope {unsigned fact;const XL6_AstNode* node;uint64_t outer,children;int function,body,blocked;} xls_entity_scope;
typedef struct xls_entity_build {xls_entity_view* view;xmap symbols,types,badSymbols,badTypes;size_t bindings;int failed;} xls_entity_build;
xls_entity_view* xlsEntityViewRetain(xls_entity_view* view){if(view)xrtAtomic32FetchAdd(&view->references,1,XMEMORY_RELAXED);return view;}
void xlsEntityViewRelease(xls_entity_view* view)
{if(view && xrtAtomic32FetchSub(&view->references,1,XMEMORY_ACQ_REL)==1){xrtArrayUnit(&view->symbols);xrtArrayUnit(&view->types);xrtArrayUnit(&view->reverseSymbols);xrtArrayUnit(&view->versions);xlsModuleGraphRelease(view->modules);xlsEntityStoreRelease(view->store);xrtFree(view);}}
xls_entity_view_info xlsEntityViewInfo(const xls_entity_view* view)
{return view ? (xls_entity_view_info){view->symbols.Count,view->types.Count,view->ambiguous,view->bytes,xlsEntityDomainIdentity(view->domain)} : (xls_entity_view_info){0};}
const xls_entity_store* xlsEntityViewStore(const xls_entity_view* view){return view ? view->store : NULL;}
const xls_module_graph* xlsEntityViewModules(const xls_entity_view* view){return view ? view->modules : NULL;}
static uint64_t xlsEntityFind(const xarray* rows,uint32_t native)
{
 if(!native || !rows)return 0;
 size_t lo=0,hi=rows->Count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;const xls_entity_binding* row=xrtArrayConstGet(rows,mid);if(row->native<native)lo=mid+1;else hi=mid;}
 const xls_entity_binding* row=xrtArrayConstGet(rows,lo);return row && row->native==native && !row->ambiguous ? row->stable : 0;
}
uint64_t xlsEntityViewSymbol(const xls_entity_view* view,uint32_t symbol){return xlsEntityFind(view ? &view->symbols : NULL,symbol);}
uint32_t xlsEntityViewNativeSymbol(const xls_entity_view* view,uint64_t identity)
{
 if(!view || !identity)return 0;
 size_t lo=0,hi=view->reverseSymbols.Count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;const xls_entity_binding* r=xrtArrayConstGet(&view->reverseSymbols,mid);if(r->stable<identity)lo=mid+1;else hi=mid;}
 const xls_entity_binding* r=xrtArrayConstGet(&view->reverseSymbols,lo);return r && r->stable==identity && !r->ambiguous ? r->native : 0;
}
uint64_t xlsEntityViewType(const xls_entity_view* view,uint32_t type){return xlsEntityFind(view ? &view->types : NULL,type);}
static int xlsEntityBytes(xstrbuf* out,const void* bytes,size_t size)
{return out->Size<=XLS_ENTITY_KEY_BYTES && size<=XLS_ENTITY_KEY_BYTES-out->Size && (!size || xrtStrBufAppend(out,(xstrview){bytes,size}));}
static int xlsEntityNumber(xstrbuf* out,uint64_t number){return xlsEntityBytes(out,&number,sizeof(number));}
static unsigned xlsEntitySource(const XL6_IndexBuild* b,const XL6_AstNode* n)
{if(n && n->source)for(size_t i=0;i<b->sourceCount;++i)if(b->sources[i].source==n->source)return b->sources[i].id;return 0;}
static int xlsEntityToken(xstrbuf* key,const XL6_IndexBuild* b,const XL6_AstNode* n)
{
 if(!n->text)return xlsEntityNumber(key,0);
 unsigned source=xlsEntitySource(b,n);const XL6_AstRange* r=&n->tokenRange;
 if(!source || source>b->report->count || !r->known || r->endOffset<=r->startOffset)return 0;
 const XL6_SourceReportItem* row=&b->report->items[source-1];size_t skip=row->size>=3 && !memcmp(row->text,"\xef\xbb\xbf",3) ? 3 : 0;
 if(!(row->kind&XL6_SOURCE_REPORT_SOURCE) || r->endOffset>row->size-skip)return 0;
 const char* text=row->text+skip+r->startOffset;size_t size=r->endOffset-r->startOffset;
 return size<=XLS_ENTITY_KEY_BYTES && xrtUtf8Valid((xstrview){text,size},NULL) && xlsEntityNumber(key,size+1) && xlsEntityBytes(key,text,size);
}
static int xlsEntityOriginalHeader(xstrbuf* key,const XL6_IndexBuild* b,const XL6_IndexFact* f)
{
 const XL6_AstNode* n=f->node;size_t end=n->range.endOffset,bodyEnd=0;
 if(!n->range.known || !f->source || f->source>b->report->count)return 0;
 if(n->kind==XL6_AST_FUNC || n->kind==XL6_AST_ASYNC_FUNC || n->kind==XL6_AST_EXPORT_FUNC){if(!xlsSyntaxFunctionHeader(n,&end,&bodyEnd))return 0;}
 const XL6_SourceReportItem* row=&b->report->items[f->source-1];size_t skip=row->size>=3 && !memcmp(row->text,"\xef\xbb\xbf",3) ? 3 : 0;
 if(!(row->kind&XL6_SOURCE_REPORT_SOURCE) || end<n->range.startOffset || end>row->size-skip)return 0;
 size_t size=end-n->range.startOffset;const char* text=row->text+skip+n->range.startOffset;
 return size && size<=XLS_ENTITY_KEY_BYTES && xrtUtf8Valid((xstrview){text,size},NULL) && xlsEntityNumber(key,(uint64_t)n->kind+1) && xlsEntityNumber(key,n->flags) && xlsEntityNumber(key,size) && xlsEntityBytes(key,text,size);
}
static int xlsEntityCallable(const XL6_AstNode* n)
{return n && (n->kind==XL6_AST_FUNC || n->kind==XL6_AST_ASYNC_FUNC || n->kind==XL6_AST_EXPORT_FUNC || n->kind==XL6_AST_EXTERN_FUNC || n->kind==XL6_AST_NATIVE_EXTERN_FUNC || n->kind==XL6_AST_ANON_FUNC);}
static int xlsEntityContainer(const XL6_AstNode* n)
{return n && (n->kind==XL6_AST_CLASS || n->kind==XL6_AST_PROTOCOL || n->kind==XL6_AST_ENUM || n->kind==XL6_AST_UNION);}
static int xlsEntityMember(const XL6_AstNode* n)
{return n && (xlsEntityCallable(n) || xlsEntityContainer(n) || n->kind==XL6_AST_FIELD || n->kind==XL6_AST_ENUM_VARIANT || n->kind==XL6_AST_VAR_DECL || n->kind==XL6_AST_CONST_DECL || n->kind==XL6_AST_EXPORT_VAR || n->kind==XL6_AST_EXTERN_VAR || n->kind==XL6_AST_EXPORT_CONST || n->kind==XL6_AST_TYPE_ALIAS || n->kind==XL6_AST_EXTERN_TYPE_ALIAS || n->kind==XL6_AST_GET || n->kind==XL6_AST_LET || n->kind==XL6_AST_CREATE);}
static int xlsEntityHeader(xstrbuf* key,const XL6_IndexBuild* b,const XL6_AstNode* n,size_t depth)
{
 if(!n)return xlsEntityNumber(key,0);
 if(depth>128 || (n->childCount && !n->children))return 0;
 if(!xlsEntityNumber(key,(uint64_t)n->kind+1) || !xlsEntityNumber(key,n->flags) || !xlsEntityToken(key,b,n) || !xlsEntityNumber(key,n->literalDecoded) || !xlsEntityNumber(key,n->literalScalar) || !xlsEntityNumber(key,n->literalSize) || (n->literalSize && (!n->literalData || !xlsEntityBytes(key,n->literalData,n->literalSize))))return 0;
 size_t count=0;for(size_t i=0;i<n->childCount;++i){const XL6_AstNode* child=n->children[i];if(child && (child->kind==XL6_AST_BLOCK || (xlsEntityContainer(n) && xlsEntityMember(child))))continue;++count;}
 if(!xlsEntityNumber(key,count))return 0;
 for(size_t i=0;i<n->childCount;++i){const XL6_AstNode* child=n->children[i];if(child && (child->kind==XL6_AST_BLOCK || (xlsEntityContainer(n) && xlsEntityMember(child))))continue;if(!xlsEntityHeader(key,b,child,depth+1))return 0;}
 return 1;
}
static uint64_t xlsEntityDeclaration(xls_entity_view* view,const XL6_IndexBuild* b,const XL6_IndexFact* f,uint64_t owner,unsigned domain,int group)
{
 const XL6_AstNode* n=f->node;
 if(!f->selection || !f->source || f->selectionSource!=f->source || !f->selection->tokenRange.known || f->selection->tokenRange.endOffset<=f->selection->tokenRange.startOffset)return 0;
 uint64_t module=xlsModuleGraphSource(view->modules,f->selectionSource);if(!module)return 0;
 xstrbuf key={0};int okay=xlsEntityNumber(&key,1) && xlsEntityNumber(&key,domain) && xlsEntityNumber(&key,owner) && xlsEntityNumber(&key,group);
 if(okay)okay=group ? xlsEntityToken(&key,b,n) : xlsEntityCallable(n) || n->kind==XL6_AST_PARAM ? xlsEntityOriginalHeader(&key,b,f) : xlsEntityHeader(&key,b,n,0);
 if(okay && !group){size_t bytes=f->canonical ? strlen(f->canonical) : 0;okay=xlsEntityNumber(&key,f->canonical!=NULL) && xlsEntityNumber(&key,bytes) && xlsEntityBytes(&key,f->canonical,bytes);}
 uint64_t result=okay ? xlsEntityStoreIntern(view->store,view->domain,module,key.Data,key.Size) : 0;xrtStrBufFree(&key);return result;
}
static int xlsEntityBind(xls_entity_build* build,int type,uint32_t native,uint64_t stable)
{
 if(!native || !stable)return 1;
 xmap *map=type ? &build->types : &build->symbols,*bad=type ? &build->badTypes : &build->badSymbols;
 xbytesview key={(const unsigned char*)&native,sizeof(native)};xls_entity_binding* old=xrtMapGet(map,key);
 if(old){if(old->stable!=stable)old->ambiguous=1;}
 else{
  if(build->bindings>=65536)return 0;
  xls_entity_binding row={native,stable,0};if(!xrtMapSet(map,key,&row))return 0;++build->bindings;
 }
 /* Separate reverse proof:two native identities cannot silently collapse
  * onto one external key in the same checked compilation. Mark BOTH unknown. */
 xbytesview reverse={(const unsigned char*)&stable,sizeof(stable)};uint32_t* previous=xrtMapGet(bad,reverse);
 if(previous){if(*previous!=native)*previous=0;}else if(!xrtMapSet(bad,reverse,&native))return 0;
 return 1;
}
static int xlsEntityBindingCompare(const void* a,const void* b)
{const xls_entity_binding *x=a,*y=b;return (x->native>y->native)-(x->native<y->native);}
static int xlsEntityReverseCompare(const void* a,const void* b)
{const xls_entity_binding *x=a,*y=b;return (x->stable>y->stable)-(x->stable<y->stable);}
static int xlsEntityReverseBuild(xls_entity_view* view)
{
 if(!xrtArrayInit(&view->reverseSymbols,sizeof(xls_entity_binding)))return 0;
 for(size_t i=0;i<view->symbols.Count;++i){const xls_entity_binding* r=xrtArrayConstGet(&view->symbols,i);if(!r->ambiguous && !xrtArrayPush(&view->reverseSymbols,r))return 0;}
 return xrtArraySort(&view->reverseSymbols,xlsEntityReverseCompare);
}
static int xlsEntityFinishMap(xls_entity_build* build,int type)
{
 xmap *map=type ? &build->types : &build->symbols,*bad=type ? &build->badTypes : &build->badSymbols;xarray* rows=type ? &build->view->types : &build->view->symbols;
 xmapiter iterator;if(!xrtMapIterBegin(map,&iterator))return 0;xls_entity_binding* row;int okay=1;
 while((row=xrtMapIterNext(&iterator,NULL))!=NULL){uint32_t* proof=xrtMapGet(bad,(xbytesview){(const unsigned char*)&row->stable,sizeof(row->stable)});xls_entity_binding current=*row;if(!proof || !*proof)current.ambiguous=1;if(current.ambiguous)++build->view->ambiguous;if(!xrtArrayPush(rows,&current)){okay=0;break;}}
 xrtMapIterEnd(&iterator);return okay && xrtArraySort(rows,xlsEntityBindingCompare);
}
static xls_context* xlsEntityContext(const XL6_IndexBuild* b,const xls_module_graph* graph)
{
 const xls_context* configured=xlsModuleGraphContext(graph);if(configured)return xlsContextRetain((xls_context*)configured);
 const char* entry=NULL;for(size_t i=0;i<xlsModuleGraphInfo(graph).modules;++i){xls_module_node node;if(xlsModuleGraphNode(graph,i,&node) && (node.roles&XLS_MODULE_ENTRY)){entry=node.path;break;}}
 char* directory=entry ? xrtPathAbs(".") : NULL;if(!directory)return NULL;
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=directory,.analysisProfile="native-entity-index-v1",.targetKind="exe",.packagePath=b->report->packagePathObserved ? b->report->packagePath : NULL};xls_context* result=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xrtFree(directory);return result;
}
#include "analysis/frontend_entity_versions.inc"
static xls_entity_view* xlsIndexEntitiesBuild(const XL6_IndexBuild* b,xls_module_graph* graph)
{
 if(!b || !graph)return NULL;
 xls_entity_view* view=xrtCalloc(1,sizeof(*view));if(!view)return NULL;xrtAtomic32Init(&view->references,1);view->modules=xlsModuleGraphRetain(graph);view->store=xlsModuleStoreEntities((xls_module_store*)xlsModuleGraphStore(graph));xls_context* context=view->store ? xlsEntityContext(b,graph) : NULL;view->domain=context ? xlsEntityStoreDomain(view->store,context) : NULL;xlsContextRelease(context);
 xls_entity_build build={.view=view};int okay=view->domain && xrtArrayInit(&view->symbols,sizeof(xls_entity_binding)) && xrtArrayInit(&view->types,sizeof(xls_entity_binding)) && xrtMapInit(&build.symbols,sizeof(xls_entity_binding)) && xrtMapInit(&build.types,sizeof(xls_entity_binding)) && xrtMapInit(&build.badSymbols,sizeof(uint32_t)) && xrtMapInit(&build.badTypes,sizeof(uint32_t));
 xls_entity_scope scopes[257]={{0}};size_t depth=0;
 for(size_t i=0;okay && i<b->count;++i){
  if(!(i%64) && xlsFrontendCancelPoll()){okay=0;break;}
  const XL6_IndexFact* f=&b->facts[i];const XL6_AstNode* n=f->node;
  while(depth && scopes[depth-1].fact!=f->parent)--depth;
  if((f->parent && !depth) || depth>=256){okay=0;break;}
  const xls_entity_scope* parent=depth ? &scopes[depth-1] : NULL;xls_entity_scope current={.fact=(unsigned)i+1,.node=n,.outer=parent ? parent->children : 0,.children=parent ? parent->children : 0,.function=parent ? parent->function : 0,.body=parent ? parent->body : 0,.blocked=parent ? parent->blocked : 0};
  if(n->kind==XL6_AST_BLOCK && current.function)current.body=1;
  int group=parent && xlsEntityCallable(parent->node) && n->kind==XL6_AST_NAME && parent->node->childCount && parent->node->children[0]==n;
  uint64_t value=0,type=0;
  if(!current.body && !current.blocked && (n->semanticDeclaredSymbolId || f->declaredType)){
   uint64_t owner=group ? parent->outer : current.outer;
   if(n->semanticDeclaredSymbolId)value=xlsEntityDeclaration(view,b,f,owner,1,group);
   if(f->declaredType)type=xlsEntityDeclaration(view,b,f,owner,2,0);
   if(!xlsEntityBind(&build,0,n->semanticDeclaredSymbolId,value) || !xlsEntityBind(&build,1,f->declaredType,type)){okay=0;break;}
  }
  if(xlsEntityCallable(n) || xlsEntityContainer(n)){current.function|=xlsEntityCallable(n);current.children=type ? type : value;if(!current.children)current.blocked=1;}
  scopes[depth++]=current;
 }
 if(okay)okay=xlsEntityFinishMap(&build,0) && xlsEntityFinishMap(&build,1) && xlsEntityReverseBuild(view);
 view->bytes=sizeof(*view)+(view->symbols.Capacity+view->types.Capacity+view->reverseSymbols.Capacity)*sizeof(xls_entity_binding);
 if(view->bytes>XLS_ENTITY_VIEW_BYTES)okay=0;
 if(okay)xlsEntityVersionsBuild(view,b);
 xrtMapUnit(&build.symbols);xrtMapUnit(&build.types);xrtMapUnit(&build.badSymbols);xrtMapUnit(&build.badTypes);
 if(!okay){xlsEntityViewRelease(view);return NULL;}return view;
}
