#include "editor/workspace_edit_positions.h"
#include "analysis/text_span.h"
#include <string.h>
typedef struct position_edit {size_t start,end,afterStart,afterEnd;} position_edit;
typedef struct position_document {const xls_text *before,*after;size_t first,count;} position_document;
struct xide_edit_positions {xide_workspace_edit* plan;position_document* documents;position_edit* edits;size_t count,bytes;};
size_t xideEditPositionsStorage(const xide_edit_positions* p){return p?p->bytes:0;}
void xideEditPositionsDestroy(xide_edit_positions* p)
{if(p){xideWorkspaceEditRelease(p->plan);xrtFree(p->documents);xrtFree(p->edits);xrtFree(p);}}
xide_edit_positions* xideEditPositionsPrepare(xide_workspace_edit* plan,size_t budget,
 int (*cancelled)(void*),void* user,enum xide_workspace_edit_error* error)
{
 enum xide_workspace_edit_error why=XIDE_WORKSPACE_EDIT_ARGUMENT;xide_workspace_edit_info info;xide_edit_positions* p=NULL;
 if(!xideWorkspaceEditInfo(plan,&info))goto done;
 size_t limit=budget?budget:128u*1024u*1024u,bytes=sizeof(*p);
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(limit>256u*1024u*1024u || info.documents>512 || info.edits>65536 || info.bytes>limit || bytes>limit-info.bytes)goto done;
 bytes+=info.bytes;size_t documentBytes=(info.documents?info.documents:1)*sizeof(position_document),editBytes=(info.edits?info.edits:1)*sizeof(position_edit);
 if(documentBytes>limit-bytes)goto done;
 bytes+=documentBytes;if(editBytes>limit-bytes)goto done;
 why=XIDE_WORKSPACE_EDIT_CANCELLED;if(cancelled && cancelled(user))goto done;
 why=XIDE_WORKSPACE_EDIT_MEMORY;p=xrtCalloc(1,sizeof(*p));if(!p)goto done;p->plan=xideWorkspaceEditRetain(plan);p->count=info.documents;p->bytes=bytes+editBytes;
 p->documents=xrtCalloc(1,documentBytes);if(!p->documents)goto done;
 p->edits=xrtMalloc(editBytes);if(!p->edits)goto done;
 size_t rows=0;
 for(size_t i=0;i<info.documents;++i){
  why=XIDE_WORKSPACE_EDIT_CANCELLED;if(cancelled && cancelled(user))goto done;
  xide_workspace_edit_document d;why=XIDE_WORKSPACE_EDIT_ARGUMENT;if(!xideWorkspaceEditDocumentAt(plan,i,&d))goto done;
  position_document* target=p->documents+i;*target=(position_document){d.before,d.after,rows,0};if(!d.changed)continue;
  const char *before=xlsTextData(d.before),*after=xlsTextData(d.after);
  for(size_t j=0;j<d.count;++j){
   why=XIDE_WORKSPACE_EDIT_CANCELLED;if(cancelled && cancelled(user))goto done;
   xide_workspace_text_edit e;why=XIDE_WORKSPACE_EDIT_ARGUMENT;if(!xideWorkspaceEditAt(plan,d.first+j,&e))goto done;
   xls_text_span span;int changed=xlsTextSpanDiff(before+e.start.offset,(size_t)(e.end.offset-e.start.offset),after+e.afterStart,e.afterEnd-e.afterStart,cancelled,user,&span);
   why=XIDE_WORKSPACE_EDIT_CANCELLED;if(changed<0)goto done;
   if(changed){p->edits[rows++]=(position_edit){(size_t)e.start.offset+span.start,(size_t)e.start.offset+span.beforeEnd,e.afterStart+span.start,e.afterStart+span.afterEnd};++target->count;}
  }
 }
 why=XIDE_WORKSPACE_EDIT_CANCELLED;if(cancelled && cancelled(user))goto done;why=XIDE_WORKSPACE_EDIT_OK;
done:
 if(why!=XIDE_WORKSPACE_EDIT_OK){xideEditPositionsDestroy(p);p=NULL;}if(error)*error=why;return p;
}
int xideEditPositionMap(const xide_edit_positions* p,size_t document,size_t offset,
 enum xide_edit_affinity affinity,size_t* output)
{
 if(!p || !output || document>=p->count || (affinity!=XIDE_EDIT_LEFT && affinity!=XIDE_EDIT_RIGHT))return 0;
 const position_document* d=p->documents+document;size_t size=xlsTextSize(d->before);
 if(offset>size || !xlsTextByteBoundary(xlsTextData(d->before),size,offset))return 0;
 const position_edit* rows=p->edits+d->first;size_t first=0,last=d->count,result=offset;
 if(affinity==XIDE_EDIT_LEFT){
  while(first<last){size_t mid=first+(last-first)/2;if(rows[mid].end<offset)first=mid+1;else last=mid;}
  if(first<d->count && rows[first].start<=offset)result=offset==rows[first].end && rows[first].start!=rows[first].end?rows[first].afterEnd:rows[first].afterStart;
  else if(first)result=rows[first-1].afterEnd+(offset-rows[first-1].end);
 }else{
  while(first<last){size_t mid=first+(last-first)/2;if(rows[mid].start<=offset)first=mid+1;else last=mid;}
  if(first){const position_edit* e=rows+first-1;result=e->afterEnd+(offset>e->end?offset-e->end:0);}
 }
 if(result>xlsTextSize(d->after))return 0;
 *output=result;return 1;
}
int xideEditSelectionMap(const xide_edit_positions* p,size_t document,
 xide_edit_selection original,xide_edit_selection* output)
{
 if(!output)return 0;
 int forward=original.anchor<original.caret;xide_edit_selection result;
 enum xide_edit_affinity anchor=forward?XIDE_EDIT_LEFT:XIDE_EDIT_RIGHT,caret=original.anchor>original.caret?XIDE_EDIT_LEFT:XIDE_EDIT_RIGHT;
 if(!xideEditPositionMap(p,document,original.anchor,anchor,&result.anchor) || !xideEditPositionMap(p,document,original.caret,caret,&result.caret))return 0;
 *output=result;return 1;
}
