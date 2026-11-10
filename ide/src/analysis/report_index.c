#include "analysis/report_index.h"
#include "analysis/frontend_context.h"
#include "analysis/input_topology.h"
#include "src/compile/xl6_source_report.h"
#include <xrt.h>
#include <string.h>
#ifndef XLS_REPORT_INDEX_ALLOC
#define XLS_REPORT_INDEX_ALLOC xrtCalloc
#endif
typedef struct report_node {
 xavlnode tree;struct report_node* next;xls_input_path_key key;
 size_t source,probe;
} report_node;
typedef struct report_lookup {
 struct report_lookup* next;const XL6_SourceReport* report;
 xavl tree;report_node* nodes;size_t sources,probes;
} report_lookup;
struct xls_report_indexes {
 report_lookup* reports;xls_report_index_work work;size_t bytes;
 int disabled,busy,compareFailed;
};
/* Immutable sentinel also makes a failed initial allocation a one-shot
 * optional refusal. It is never modified/freed and may be shared by threads. */
static const xls_report_indexes declined={.disabled=1,.work={.declines=1}};
static int reportCharge(xls_report_indexes* s,size_t bytes)
{if(bytes>8u*1024u*1024u-s->bytes)return 0;s->bytes+=bytes;if(s->bytes>s->work.peakBytes)s->work.peakBytes=s->bytes;return 1;}
static int reportCompare(const void* key,const xavlnode* node,void* user)
{
 xls_report_indexes* s=user;int okay=0;++s->work.comparisons;
 int result=xlsInputPathCompare(key,&((const report_node*)node)->key,&okay);
 if(!okay)s->compareFailed=1;
 return result;
}
static void reportDrop(xls_report_indexes* s,report_lookup* r)
{
 report_node* n=r->nodes;
 /* Do not compare keys or follow AVL links during destruction: optional
  * comparator/insert failure may have left a deliberately unused tree. */
 while(n){report_node* next=n->next;s->bytes-=sizeof(*n)+n->key.bytes;xrtFree(n->key.data);xrtFree(n);n=next;}
 s->bytes-=sizeof(*r);xrtFree(r);
}
void xlsReportIndexesDestroy(xls_report_indexes* s)
{
 if(!s || s==&declined)return;
 report_lookup* r=s->reports;while(r){report_lookup* next=r->next;reportDrop(s,r);r=next;}xrtFree(s);
}
void xlsReportIndexesForget(xls_report_indexes* s,const XL6_SourceReport* report)
{
 if(!s || s==&declined)return;
 report_lookup** link=&s->reports;while(*link){report_lookup* r=*link;if(r->report==report){*link=r->next;reportDrop(s,r);return;}link=&r->next;}
}
void xlsReportIndexesWork(const xls_report_indexes* s,xls_report_index_work* out)
{if(out)*out=s?s->work:(xls_report_index_work){0};}
static report_lookup* reportState(xls_report_indexes* s,const XL6_SourceReport* report)
{
 for(report_lookup* r=s->reports;r;r=r->next)if(r->report==report)return r;
 report_lookup* r=XLS_REPORT_INDEX_ALLOC(1,sizeof(*r));if(!r)return NULL;
 if(!reportCharge(s,sizeof(*r))){xrtFree(r);return NULL;}
 r->report=report;r->next=s->reports;s->reports=r;
 if(!xrtAVLInit(&r->tree))return NULL;
 return r;
}
static report_node* reportAdd(xls_report_indexes* s,report_lookup* r,const char* path)
{
 xls_input_path_key key=xlsInputPathPrepare(path,1);if(!key.data)return NULL;
 report_node* node=(report_node*)xrtAVLFind(&r->tree,&key,reportCompare,s);
 if(s->compareFailed){xrtFree(key.data);return NULL;}
 if(node){xrtFree(key.data);return node;}
 node=XLS_REPORT_INDEX_ALLOC(1,sizeof(*node));if(!node){xrtFree(key.data);return NULL;}
 if(!reportCharge(s,sizeof(*node)+key.bytes)){xrtFree(key.data);xrtFree(node);return NULL;}
 node->key=key;node->source=node->probe=SIZE_MAX;node->next=r->nodes;r->nodes=node;xrtAVLNodeInit(&node->tree);
 bool added=false;xavlnode* found=xrtAVLInsert(&r->tree,&node->tree,&node->key,reportCompare,s,&added);
 return !s->compareFailed && added && found==&node->tree?node:NULL;
}
int xlsReportIndexLookup(XL6_SourceReport* report,const char* path,enum xls_report_index_kind kind,size_t* index)
{
 if(index)*index=SIZE_MAX;
 if(!report || !path || !index || (kind!=XLS_REPORT_SOURCE && kind!=XLS_REPORT_PROBE))return 2;
 /* Tiny reports keep the original scan and allocate no sidecar. */
 if(report->count<32 && report->probeCount<128)return 2;
 xls_report_indexes** slot=xlsFrontendReportIndexSlot();if(!slot)return 2;
 if(!*slot){
  *slot=(xls_report_indexes*)&declined;
  xls_report_indexes* fresh=XLS_REPORT_INDEX_ALLOC(1,sizeof(*fresh));if(!fresh)return 2;
  fresh->bytes=sizeof(*fresh);fresh->work.peakBytes=sizeof(*fresh);*slot=fresh;
 }
 xls_report_indexes* s=*slot;if(s==&declined)return 2;
 ++s->work.lookups;if(s->disabled || s->busy)return 2;s->busy=1;s->compareFailed=0;
 int result=2;xls_input_path_key key={0};report_lookup* r=reportState(s,report);if(!r)goto decline;
 if(report->count<r->sources || report->probeCount<r->probes)goto decline;
 while(r->sources<report->count){size_t row=r->sources;report_node* n=reportAdd(s,r,report->items[row].path);if(!n)goto decline;if(n->source==SIZE_MAX)n->source=row;++r->sources;++s->work.preparedRows;}
 while(r->probes<report->probeCount){size_t row=r->probes;report_node* n=reportAdd(s,r,report->probes[row].path);if(!n)goto decline;if(n->probe==SIZE_MAX)n->probe=row;++r->probes;++s->work.preparedRows;}
 key=xlsInputPathPrepare(path,1);if(!key.data)goto decline;
 report_node* n=(report_node*)xrtAVLFind(&r->tree,&key,reportCompare,s);if(s->compareFailed)goto decline;
 *index=n?(kind==XLS_REPORT_SOURCE?n->source:n->probe):SIZE_MAX;
 result=*index==SIZE_MAX?0:1;++s->work.indexed;goto done;
decline:s->disabled=1;++s->work.declines;
done:xrtFree(key.data);s->busy=0;return result;
}
