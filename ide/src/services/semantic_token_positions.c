#include "services/semantic_token_positions.h"
#include "services/semantic_definition.h"
#include <xrt.h>
#include <stdlib.h>
#include <limits.h>
typedef struct point {int line,character;} point;
typedef struct endpoint {int source,offset,line,column;point* output;} endpoint;
typedef struct token_pair {point start,end;} token_pair;
static int compareEndpoint(const void* a,const void* b)
{const endpoint *x=a,*y=b;return x->source!=y->source?(x->source>y->source)-(x->source<y->source):(x->offset>y->offset)-(x->offset<y->offset);}
#define XIDE_COORDINATE_CANCELLABLE_ONLY
#include "services/semantic_coordinate_sorted.inc"
#undef XIDE_COORDINATE_CANCELLABLE_ONLY
struct xide_token_positions {xatomic32 references;size_t count;xide_token_position* rows;xide_token_position_work work;};
typedef struct position_stop {int (*callback)(void*);void* user;int stopped;} position_stop;
static int positionStopped(void* user){position_stop* stop=user;if(!stop->stopped)stop->stopped=stop->callback(stop->user);return stop->stopped;}
xide_token_positions* xideTokenPositionsCreate(const xls_semantic_check* check,size_t source,
 int prefix,size_t budget,int (*cancelled)(void*),void* user,enum xls_semantic_tokens_error* error)
{
 const size_t limit=16u*1024u*1024u;budget=budget?budget:limit;
 enum xls_semantic_tokens_error reason=XLS_TOKENS_ARGUMENT;
 xide_token_positions* result=NULL;token_pair* pairs=NULL;endpoint* ends=NULL;xls_semantic_tokens* tokens=NULL;
 if(prefix<0 || prefix>1 || budget>limit || budget<sizeof(*result))goto done;
 tokens=xlsSemanticTokensCreate(check,source,budget,cancelled,user,&reason);if(!tokens)goto done;
 xls_input_source input;if(!xlsSemanticCheckSource(check,source,&input)){reason=XLS_TOKENS_NO_INDEX;goto done;}
 size_t count=xlsSemanticTokensCount(tokens);xls_semantic_tokens_work core=xlsSemanticTokensWork(tokens);
 size_t per=sizeof(xide_token_position)+sizeof(token_pair)+2*sizeof(endpoint);
 reason=XLS_TOKENS_LIMIT;
 if(core.bytes>budget-sizeof(*result) || count>(budget-sizeof(*result)-core.bytes)/per)goto done;
 result=xrtCalloc(1,sizeof(*result));if(!result){reason=XLS_TOKENS_MEMORY;goto done;}
 xrtAtomic32Init(&result->references,1);result->count=count;
 result->work=(xide_token_position_work){.classification=core,.bytes=sizeof(*result)+count*sizeof(xide_token_position),.peakBytes=core.bytes+sizeof(*result)+count*per};
 if(result->work.peakBytes<core.peakBytes)result->work.peakBytes=core.peakBytes;
 if(count){result->rows=xrtCalloc(count,sizeof(*result->rows));pairs=xrtCalloc(count,sizeof(*pairs));ends=xrtCalloc(count*2,sizeof(*ends));if(!result->rows || !pairs || !ends){reason=XLS_TOKENS_MEMORY;goto done;}}
 const xls_semantic_token* rows=xlsSemanticTokensData(tokens);
 for(size_t i=0;i<count;++i){
  if(!(i%256) && cancelled && cancelled(user)){reason=XLS_TOKENS_CANCELLED;goto done;}
  xls_index_range r=rows[i].range;
  if(r.startOffset>INT_MAX || r.endOffset>INT_MAX){reason=XLS_TOKENS_FACTS;goto done;}
  ends[i*2]=(endpoint){1,(int)r.startOffset,r.startLine,r.startColumn,&pairs[i].start};
  ends[i*2+1]=(endpoint){1,(int)r.endOffset,r.endLine,r.endColumn,&pairs[i].end};
 }
 xide_semantic_source text={input.path,input.text,input.kind,prefix};
 position_stop stop={cancelled,user,0};
 if(count && !convertCancellable(ends,count*2,&text,cancelled?positionStopped:NULL,&stop,&result->work.scannedBytes)){reason=stop.stopped?XLS_TOKENS_CANCELLED:XLS_TOKENS_FACTS;goto done;}
 for(size_t i=0;i<count;++i){
  if(pairs[i].start.line!=pairs[i].end.line || pairs[i].end.character<=pairs[i].start.character){reason=XLS_TOKENS_FACTS;goto done;}
  result->rows[i]=(xide_token_position){pairs[i].start.line,pairs[i].start.character,pairs[i].end.character-pairs[i].start.character,rows[i].kind,rows[i].modifiers};
 }
 if(cancelled && cancelled(user)){reason=XLS_TOKENS_CANCELLED;goto done;}
 if(!xlsSemanticCheckTypedIndex(check)){reason=XLS_TOKENS_NO_INDEX;goto done;}
 reason=XLS_TOKENS_OK;
done:
 xlsSemanticTokensRelease(tokens);xrtFree(pairs);xrtFree(ends);
 if(reason){xideTokenPositionsRelease(result);result=NULL;}
 if(error)*error=reason;
 return result;
}
xide_token_positions* xideTokenPositionsRetain(xide_token_positions* p){if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xideTokenPositionsRelease(xide_token_positions* p){if(p && xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)==1){xrtFree(p->rows);xrtFree(p);}}
size_t xideTokenPositionsCount(const xide_token_positions* p){return p?p->count:0;}
const xide_token_position* xideTokenPositionsData(const xide_token_positions* p){return p?p->rows:NULL;}
xide_token_position_work xideTokenPositionsWork(const xide_token_positions* p){return p?p->work:(xide_token_position_work){0};}
