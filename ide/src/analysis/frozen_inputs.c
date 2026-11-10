#include "analysis/frozen_inputs.h"
#include "analysis/input_topology.h"
#include "analysis/frontend_input_usage.h"
#include "src/compile/xl6_source_overlay.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef XLS_FROZEN_ALLOC
#define XLS_FROZEN_ALLOC xrtCalloc
#endif
typedef struct frozen_item {XL6_SourceOverlayItem item;xls_input_path_key key;} frozen_item;
struct xls_frozen_inputs {xatomic32 references;size_t count,bytes;frozen_item items[];};
static _Thread_local int frozenSortFailed;
static int frozenOrder(const void* a,const void* b)
{int okay=0;int order=xlsInputPathCompare(&((const frozen_item*)a)->key,&((const frozen_item*)b)->key,&okay);if(!okay)frozenSortFailed=1;return order;}
static int frozenCharge(size_t* used,size_t add)
{const size_t limit=128u*1024u*1024u;if(*used>limit || add>limit-*used)return 0;*used+=add;return 1;}
static int frozenStop(int (*cancelled)(void*),void* user)
{return cancelled && cancelled(user);}
static void frozenError(char* error,size_t capacity,const char* message)
{if(error && capacity)snprintf(error,capacity,"%s",message);}
xls_frozen_inputs* xlsFrozenInputsRetain(xls_frozen_inputs* inputs)
{if(inputs)xrtAtomic32FetchAdd(&inputs->references,1,XMEMORY_RELAXED);return inputs;}
void xlsFrozenInputsRelease(xls_frozen_inputs* inputs)
{
 if(!inputs || xrtAtomic32FetchSub(&inputs->references,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;i<inputs->count;++i){xrtFree(inputs->items[i].item.path);xrtFree(inputs->items[i].item.text);xrtFree(inputs->items[i].key.data);}xrtFree(inputs);
}
size_t xlsFrozenInputsBytes(const xls_frozen_inputs* inputs){return inputs?inputs->bytes:0;}
xls_frozen_inputs* xlsFrozenInputsCreate(const xls_semantic_input* inputs,size_t count,
 int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
 xls_frozen_inputs* result=NULL;const char* why="invalid frozen native inputs";size_t bytes=0;
 if(error && capacity)*error=0;
 if(!inputs || !count || count>XLS_FROZEN_INPUT_FILES)goto failed;
 if(frozenStop(cancelled,user)){why="frozen native input capture cancelled";goto failed;}
 result=XLS_FROZEN_ALLOC(1,sizeof(*result)+count*sizeof(frozen_item));why="cannot allocate frozen native inputs";if(!result)goto failed;
 xrtAtomic32Init(&result->references,1);result->bytes=sizeof(*result)+count*sizeof(frozen_item);
 for(size_t i=0;i<count;++i){
  if(frozenStop(cancelled,user)){why="frozen native input capture cancelled";goto failed;}
  const xls_semantic_input* in=inputs+i;why="invalid frozen native path or UTF8 source";
  if(!in->path || !xrtPathIsAbs(in->path) || !in->text || in->size>XL6_SOURCE_OVERLAY_BYTES ||
     memchr(in->text,0,in->size) || !xrtUtf8Valid((xstrview){in->text,in->size},NULL))goto failed;
  size_t bom=in->size>=3 && !memcmp(in->text,"\xef\xbb\xbf",3)?3:0,size=in->size-bom;
  why="frozen native input text budget exceeded";if(size>XL6_SOURCE_REPORT_TOTAL_BYTES-bytes)goto failed;bytes+=size;
  frozen_item* row=result->items+result->count++;row->item.path=xrtPathAbs(in->path);
  why="cannot prepare frozen native path";if(!row->item.path)goto failed;
  row->key=xlsInputPathPrepare(row->item.path,1);if(!row->key.data)goto failed;
  why="frozen native input storage budget exceeded";
  if(!frozenCharge(&result->bytes,strlen(row->item.path)+1) || !frozenCharge(&result->bytes,row->key.bytes) || !frozenCharge(&result->bytes,size+1))goto failed;
  row->item.text=xrtMalloc(size+1);why="cannot copy frozen native source";if(!row->item.text)goto failed;row->item.size=size;
  for(size_t at=0;at<size;){if(frozenStop(cancelled,user)){why="frozen native input capture cancelled";goto failed;}size_t n=size-at;if(n>65536)n=65536;memcpy(row->item.text+at,in->text+bom+at,n);at+=n;}row->item.text[size]=0;
 }
 frozenSortFailed=0;qsort(result->items,count,sizeof(*result->items),frozenOrder);why="cannot compare frozen native source identities";if(frozenSortFailed)goto failed;
 for(size_t i=1;i<count;++i){int okay=0;int c=xlsInputPathCompare(&result->items[i-1].key,&result->items[i].key,&okay);
  why="duplicate or uncomparable frozen native source identity";if(!okay || !c)goto failed;}
 if(frozenStop(cancelled,user)){why="frozen native input capture cancelled";goto failed;}
 return result;
failed:xlsFrozenInputsRelease(result);frozenError(error,capacity,why);return NULL;
}
int xlsFrozenInputsLookup(const xls_frozen_inputs* inputs,const XL6_SourceOverlay* overlay,const char* path,const XL6_SourceOverlayItem** item)
{
 char* absolute=NULL;xls_input_path_key key={0};int result=-1;
 if(item)*item=NULL;
 if(!inputs || !item || !path || !*path || (overlay && overlay->report && !overlay->report->complete))goto done;
 absolute=xrtPathAbs(path);if(!absolute)goto done;key=xlsInputPathPrepare(absolute,1);if(!key.data)goto done;
 size_t low=0,high=inputs->count;
 while(low<high){size_t mid=low+(high-low)/2;int okay=0;int c=xlsInputPathCompare(&inputs->items[mid].key,&key,&okay);if(!okay)goto done;if(c<0)low=mid+1;else high=mid;}
 result=0;
 if(low<inputs->count){int okay=0;int c=xlsInputPathCompare(&inputs->items[low].key,&key,&okay);if(!okay){result=-1;goto done;}if(!c){*item=&inputs->items[low].item;result=1;}}
done:
 xrtFree(key.data);xrtFree(absolute);
 /* Frozen checks never claim a64-bit usage/topology proof, even on a miss. */
 xlsFrontendInputUsageFailure(overlay);
 if(result<0)xl6SourceReportInvalidate(overlay?overlay->report:NULL,path,"cannot resolve frozen native source identity");
 return result;
}
