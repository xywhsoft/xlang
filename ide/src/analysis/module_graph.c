#include "analysis/frontend_module_graph.h"
#include "analysis/frontend_cancel.h"
#include <xrt.h>
#include <string.h>
#define GRAPH_BYTES (8u*1024u*1024u)
#define GRAPH_NODES 65536u
#define GRAPH_EDGES 262144u
typedef struct module_node {char* path;uint64_t identity;unsigned roles,source;} module_node;
typedef struct module_edge {uint32_t from,to;} module_edge;
struct xls_module_graph {xatomic32 references;xls_module_store* store;xls_context* context;xarray nodes,edges,sources;size_t bytes,root;};
typedef struct graph_pending {xls_module_graph* graph;xmap nodes,edges;size_t labels,limit;int failed,entered,expanded;} graph_pending;
static _Thread_local xls_module_graph_scope* active;
void xlsModuleGraphDecline(void)
{graph_pending* p=active ? active->state : NULL;if(p)p->failed=1;}
static xbytesview label(const char* s){return (xbytesview){(const unsigned char*)s,strlen(s)+1};}
static size_t storage(const graph_pending* p)
{
 const xls_module_graph* g=p->graph;
 return sizeof(*p)+sizeof(*g)+xlsContextBytes(g->context)+p->labels+g->nodes.Capacity*sizeof(module_node)+g->edges.Capacity*sizeof(module_edge)+g->sources.Capacity*sizeof(uint64_t)+g->nodes.Count*sizeof(uint32_t)+g->edges.Count*sizeof(module_edge);
}
static int budget(graph_pending* p){if(storage(p)>p->limit)p->failed=1;return !p->failed;}
xls_module_graph* xlsModuleGraphRetain(xls_module_graph* g){if(g)xrtAtomic32FetchAdd(&g->references,1,XMEMORY_RELAXED);return g;}
void xlsModuleGraphRelease(xls_module_graph* g)
{
 if(!g || xrtAtomic32FetchSub(&g->references,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;i<g->nodes.Count;++i)xrtFree(((module_node*)xrtArrayGet(&g->nodes,i))->path);
 xrtArrayUnit(&g->nodes);xrtArrayUnit(&g->edges);xrtArrayUnit(&g->sources);xlsContextRelease(g->context);xlsModuleStoreRelease(g->store);xrtFree(g);
}
static void pendingFree(graph_pending* p)
{if(p){xrtMapUnit(&p->nodes);xrtMapUnit(&p->edges);xlsModuleGraphRelease(p->graph);xrtFree(p);}}
void xlsModuleGraphBegin(xls_module_graph_scope* scope,xls_module_store* store,xls_context* context,size_t bytes)
{
 if(!scope)return;
 *scope=(xls_module_graph_scope){active,NULL};active=scope;
 if(!store || !bytes)return;
 if(bytes>GRAPH_BYTES)bytes=GRAPH_BYTES;
 graph_pending* p=xrtCalloc(1,sizeof(*p));xls_module_graph* g=xrtCalloc(1,sizeof(*g));
 if(g){xrtAtomic32Init(&g->references,1);g->store=xlsModuleStoreRetain(store);g->context=xlsContextRetain(context);}
 if(p)p->graph=g;else xlsModuleGraphRelease(g);
 if(!p || !g || !xrtMapInit(&p->nodes,sizeof(uint32_t)) || !xrtMapInit(&p->edges,sizeof(unsigned char)) || !xrtArrayInit(&g->nodes,sizeof(module_node)) || !xrtArrayInit(&g->edges,sizeof(module_edge)) || !xrtArrayInit(&g->sources,sizeof(uint64_t))){pendingFree(p);return;}
 p->limit=bytes;scope->state=p;budget(p);
}
static uint32_t node(graph_pending* p,const char* path)
{
 if(p->failed)return UINT32_MAX;
 size_t bytes=path ? strlen(path)+1 : 0;
 if(bytes<2 || bytes>262145 || !xrtPathIsAbs(path) || !xrtUtf8Valid((xstrview){path,bytes-1},NULL)){p->failed=1;return UINT32_MAX;}
 uint32_t* old=xrtMapGet(&p->nodes,label(path));if(old)return *old;
 if(p->graph->nodes.Count>=GRAPH_NODES || bytes>(p->limit-p->labels)/2){p->failed=1;return UINT32_MAX;}
 module_node fresh={.path=xrtStrDup(path)};uint32_t at=(uint32_t)p->graph->nodes.Count;
 if(!fresh.path || !xrtArrayPush(&p->graph->nodes,&fresh)){xrtFree(fresh.path);p->failed=1;return UINT32_MAX;}
 p->labels+=bytes*2;
 if(!xrtMapSet(&p->nodes,label(path),&at) || !budget(p)){p->failed=1;return UINT32_MAX;}return at;
}
void xlsModuleGraphImport(char* const* stack,size_t count)
{
 graph_pending* p=active ? active->state : NULL;if(!p || p->failed)return;
 if(!stack || !count || count>GRAPH_NODES){p->failed=1;return;}
 uint32_t to=node(p,stack[count-1]);if(to==UINT32_MAX)return;
 if(count==1){if(p->entered && p->graph->root!=to){p->failed=1;return;}p->entered=1;p->graph->root=to;((module_node*)xrtArrayGet(&p->graph->nodes,to))->roles|=XLS_MODULE_ENTRY;return;}
 uint32_t from=node(p,stack[count-2]);if(from==UINT32_MAX)return;
 module_edge edge={from,to};xbytesview key={(const unsigned char*)&edge,sizeof(edge)};unsigned char one=1;
 if(xrtMapHas(&p->edges,key))return;
 if(p->graph->edges.Count>=GRAPH_EDGES || !xrtMapSet(&p->edges,key,&one) || !xrtArrayPush(&p->graph->edges,&edge) || !budget(p))p->failed=1;
}
void xlsModuleGraphExpanded(char* const* stack,size_t count,int accepted)
{
 graph_pending* p=active ? active->state : NULL;if(!p || p->failed || count!=1)return;
 module_node* root=p->entered ? xrtArrayGet(&p->graph->nodes,p->graph->root) : NULL;
 if(!stack || !root || strcmp(root->path,stack[0]))p->failed=1;else p->expanded=accepted!=0;
}
xls_module_graph* xlsModuleGraphEnd(xls_module_graph_scope* scope,const XL6_SourceReport* report)
{
 if(!scope || active!=scope)return NULL;
 active=scope->previous;graph_pending* p=scope->state;scope->state=NULL;
 if(!p)return NULL;
 xls_module_graph* g=p->graph;
 if(!p->failed && p->entered && p->expanded && report && report->complete && report->count && report->items && report->count<=512){
  if(!xrtArrayResize(&g->sources,report->count))p->failed=1;
  for(size_t i=0;!p->failed && i<report->count;++i){
   uint32_t at=node(p,report->items[i].path);if(at==UINT32_MAX)break;
   module_node* n=xrtArrayGet(&g->nodes,at);if(n->source){p->failed=1;break;}
   n->source=(unsigned)i+1;n->roles|=(unsigned)report->items[i].kind;
  }
  const xls_context_spec* context=xlsContextSpec(g->context);
  if(context)for(size_t i=0;!p->failed && i<context->lists[XLS_CONTEXT_MEMBERS].count;++i){
   const char* path=context->lists[XLS_CONTEXT_MEMBERS].items[i];uint32_t* at=xrtMapGet(&p->nodes,label(path));
   if(at)((module_node*)xrtArrayGet(&g->nodes,*at))->roles|=XLS_MODULE_DECLARED_MEMBER;
   if(!(i%64) && xlsFrontendCancelPoll())p->failed=1;
  }
  budget(p);
  for(size_t i=0;!p->failed && i<g->nodes.Count;++i){
   module_node* n=xrtArrayGet(&g->nodes,i);n->identity=xlsModuleStoreIdentity(g->store,n->path);
   if(!n->identity || (!(i%64) && xlsFrontendCancelPoll()))p->failed=1;
   if(n->source)*(uint64_t*)xrtArrayGet(&g->sources,n->source-1)=n->identity;
  }
  if(!p->failed){g->bytes=sizeof(*g)+xlsContextBytes(g->context)+p->labels/2+g->nodes.Capacity*sizeof(module_node)+g->edges.Capacity*sizeof(module_edge)+g->sources.Capacity*sizeof(uint64_t);p->graph=NULL;}
 }
 xls_module_graph* result=p->graph ? NULL : g;pendingFree(p);return result;
}
xls_module_graph_info xlsModuleGraphInfo(const xls_module_graph* g)
{return g ? (xls_module_graph_info){g->nodes.Count,g->edges.Count,g->sources.Count,g->bytes,((const module_node*)xrtArrayConstGet(&g->nodes,g->root))->identity} : (xls_module_graph_info){0};}
const xls_context* xlsModuleGraphContext(const xls_module_graph* g){return g ? g->context : NULL;}
const xls_module_store* xlsModuleGraphStore(const xls_module_graph* g){return g ? g->store : NULL;}
int xlsModuleGraphNode(const xls_module_graph* g,size_t at,xls_module_node* out)
{
 if(out)*out=(xls_module_node){0};
 if(!g || !out || at>=g->nodes.Count)return 0;
 const module_node* n=xrtArrayConstGet(&g->nodes,at);*out=(xls_module_node){n->identity,n->path,n->roles,n->source};return 1;
}
int xlsModuleGraphEdge(const xls_module_graph* g,size_t at,xls_module_edge* out)
{
 if(out)*out=(xls_module_edge){0};
 if(!g || !out || at>=g->edges.Count)return 0;
 const module_edge* e=xrtArrayConstGet(&g->edges,at);const module_node *from=xrtArrayConstGet(&g->nodes,e->from),*to=xrtArrayConstGet(&g->nodes,e->to);*out=(xls_module_edge){from->identity,to->identity};return 1;
}
uint64_t xlsModuleGraphSource(const xls_module_graph* g,size_t source)
{return g && source && source<=g->sources.Count ? *(const uint64_t*)xrtArrayConstGet(&g->sources,source-1) : 0;}
