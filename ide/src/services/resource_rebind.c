#include "services/resource_rebind.h"
#include "model/resource_text.h"
#include "editor/coordinates.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PLAN_LIMIT (128u*1024u*1024u)
typedef struct resource_row {
 char* path; char* before; char* after;
 xide_file_snapshot disk;
 xide_document* document; uint64_t id; uint32_t version;
 int affected,mask,opened,applied;
 struct saved_position* selections; int selectionCount; float scrollX,scrollY;
} resource_row;
typedef struct saved_position { xui_code_selection_t state; int anchorLine,anchorColumn,caretLine,caretColumn; } saved_position;
struct xide_resource_rebind { xide_document_store* documents; resource_row* rows; size_t count,bytes; char* source; };
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,int code,const char* text) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static const char* pathValue(const xvalue* root,int field)
{
 xvalue* v=xrtValueObjectGet(root,sv(field ? "manifest" : "icon")); xstrview text={0};
 if(xrtValueType(v)==XVALUE_OBJECT) v=xrtValueObjectGet(v,sv("path"));
 return xrtValueGetString(v,&text) && !memchr(text.Data,0,text.Size) ? text.Data : NULL;
}
static int mask(const char* body,const char* config,const char* source,int* result,xide_model_error* e)
{
 xide_resource_text parsed={0}; char* directory=xidePathDirectory(config); int okay=directory && xideResourceTextParse(&parsed,body,e); *result=0;
 for(int i=0;okay && i<2;++i) { const char* path=pathValue(parsed.projection,i); if(!path) continue;
  char* absolute=*path ? xidePathResolve(directory,path) : NULL;
  if(!absolute) okay=fail(e,XIDE_MODEL_INVALID,"资源素材路径为空或无法解析，请先修正资源配置。");
  else if(xidePathEqual(absolute,source)) *result|=1<<i;
  free(absolute);
 }
 free(directory); xideResourceTextClear(&parsed); return okay;
}
static int add(xide_resource_rebind* p,const char* path,xide_model_error* e)
{
 if(xidePathEqual(path,p->source)) return 1; /* Config's own name is handled by file_rebind. */
 for(size_t i=0;i<p->count;++i) if(xidePathEqual(p->rows[i].path,path)) {
  if(!xidePathLexicalEqual(p->rows[i].path,path)) return fail(e,XIDE_MODEL_CONFLICT,"同一资源配置通过不同硬链接路径加载，无法保证素材目录基准和别名身份；请统一配置路径后重试。");
  return 1;
 }
 resource_row row={0}; xide_text_file file={0}; xide_file_snapshot after={0};
 int okay=xideFileSnapshotInspect(path,&row.disk,e) && xideTextFileRead(path,&file,e) && xideFileSnapshotInspect(path,&after,e);
 if(okay && !xideFileSnapshotEqual(&row.disk,&after)) okay=fail(e,XIDE_MODEL_CONFLICT,"资源配置在读取时发生变化，请重新打开改名窗口。");
 row.document=xideDocumentsFind(p->documents,path);
 const char* body=row.document ? xuiCodeEditGetText(row.document->editor) : file.text;
 if(okay) okay=mask(body,path,p->source,&row.mask,e);
 if(okay && row.mask && !xideFileRenameSourceValid(path,e)) okay=0;
 if(okay && (strlen(body)>PLAN_LIMIT-p->bytes)) okay=fail(e,XIDE_MODEL_INVALID,"改名引用检查超过 128 MiB，请减少加载项目后重试。");
 if(okay) { row.path=copy(path); row.before=copy(body); row.affected=row.mask!=0;
  if(row.document) { row.id=row.document->id; row.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(row.document->editor)); }
  resource_row* rows=row.path && row.before ? realloc(p->rows,(p->count+1)*sizeof(*rows)) : NULL;
  if(!rows) okay=fail(e,XIDE_MODEL_MEMORY,"无法准备资源路径改名基线。");
  else { p->rows=rows; p->rows[p->count++]=row; p->bytes+=strlen(body); }
 }
 if(!okay) { free(row.path); free(row.before); } xideTextFileClear(&file); return okay;
}
xide_resource_rebind* xideResourceRebindPrepare(const xide_workspace* w,xide_document_store* docs,const char* source,xide_model_error* e)
{
 if(!w || !docs || !source || !*source) { fail(e,XIDE_MODEL_INVALID,"缺少工作空间、文档服务或改名路径。"); return NULL; }
 xide_resource_rebind* p=calloc(1,sizeof(*p)); if(!p) { fail(e,XIDE_MODEL_MEMORY,"无法准备资源引用检查。"); return NULL; }
 p->documents=docs; p->source=copy(source); int okay=p->source!=NULL;
 for(size_t i=0;okay && i<w->projectCount;++i) if(w->projects[i].available)
  for(size_t j=0;okay && j<w->projects[i].resources.count;++j) okay=add(p,w->projects[i].resources.items[j],e);
 if(!okay) { xideResourceRebindDestroy(p); return NULL; } return p;
}
/* Relative directory is canonicalized, then append the requested leaf literally:
   resolving the whole target would erase a case-only rename on Windows. */
static char* spelling(const char* config,const char* previous,const char* target)
{
 int absolute=previous[0]=='/' || previous[0]=='\\' || (previous[0] && previous[1]==':');
 char* result=NULL;
 if(absolute) result=copy(target);
 else {
  char *base=xidePathDirectory(config),*parent=xidePathDirectory(target),*relative=base && parent ? xidePathRelative(base,parent) : NULL;
  const char* leaf=xidePathFilename(target); size_t n=relative ? strlen(relative)+strlen(leaf)+6 : 0;
 if(n) { result=malloc(n); if(result) snprintf(result,n,"%s%s%s%s",(!strncmp(previous,"./",2) || !strncmp(previous,".\\",2)) && !(relative[0] && relative[1]==':') ? "./" : "",strcmp(relative,".") ? relative : "",strcmp(relative,".") && *relative ? "/" : "",leaf); }
  free(base); free(parent); free(relative);
 }
 if(result && strchr(previous,'\\') && !strchr(previous,'/')) for(char* at=result;*at;++at) if(*at=='/') *at='\\';
 return result;
}
int xideResourceRebindRetarget(xide_resource_rebind* p,const char* target,xide_model_error* e)
{
 if(!p || !target || !*target) return fail(e,XIDE_MODEL_INVALID,"缺少资源改名计划或目标路径。");
 for(size_t i=0;i<p->count;++i) {
  resource_row* r=&p->rows[i]; if(!r->affected) continue;
  xide_resource_text parsed={0}; xvalue* draft=NULL; char* after=NULL;
  int okay=xideResourceTextParse(&parsed,r->before,e) && (draft=xrtValueDeepClone(parsed.projection))!=NULL;
  for(int f=0;okay && f<2;++f) if(r->mask&(1<<f)) {
   const char* key=f ? "manifest" : "icon"; const char* old=pathValue(draft,f); char* path=spelling(r->path,old,target);
   xvalue* object=xrtValueObjectGet(draft,sv(key));
   okay=path && xrtValueObjectSetNew(xrtValueType(object)==XVALUE_OBJECT ? object : draft,sv(xrtValueType(object)==XVALUE_OBJECT ? "path" : key),xrtValueString(sv(path)));
   free(path);
  }
  if(okay) after=xideResourceTextSerialize(&parsed,draft,e);
  xrtValueRelease(draft); xideResourceTextClear(&parsed);
  if(!after) return fail(e,XIDE_MODEL_INVALID,"无法生成保留扩展字段的资源路径更新。");
  xrtFree(r->after); r->after=after;
 }
 return 1;
}
int xideResourceRebindValidate(const xide_resource_rebind* p,xide_model_error* e)
{
 for(size_t i=0;i<p->count;++i) {
  const resource_row* r=&p->rows[i]; xide_file_snapshot disk={0};
  if(!xideFileSnapshotInspect(r->path,&disk,e) || !xideFileSnapshotEqual(&r->disk,&disk)) return fail(e,XIDE_MODEL_CONFLICT,"资源配置的磁盘内容或身份已改变，请取消并重新打开改名窗口。");
  xide_document* d=xideDocumentsFind(p->documents,r->path);
  if(d!=r->document || (d && (!d->open || d->id!=r->id || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))!=r->version || strcmp(xuiCodeEditGetText(d->editor),r->before))))
   return fail(e,XIDE_MODEL_CONFLICT,"资源配置文档已改变，请取消并重新打开改名窗口。");
 }
 return 1;
}
size_t xideResourceRebindCount(const xide_resource_rebind* p) { size_t n=0; if(p) for(size_t i=0;i<p->count;++i) n+=p->rows[i].affected!=0; return n; }
static resource_row* at(const xide_resource_rebind* p,size_t index) { if(p) for(size_t i=0;i<p->count;++i) if(p->rows[i].affected && index--==0) return &p->rows[i]; return NULL; }
const char* xideResourceRebindPath(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r ? r->path : NULL; }
xide_document* xideResourceRebindDocument(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r ? r->document : NULL; }
int xideResourceRebindOpened(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r && r->opened; }
const char* xideResourceRebindBefore(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r ? r->before : NULL; }
const char* xideResourceRebindAfter(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r ? r->after : NULL; }
const xide_file_snapshot* xideResourceRebindDisk(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r ? &r->disk : NULL; }
int xideResourceRebindApplied(const xide_resource_rebind* p,size_t i) { resource_row* r=at(p,i); return r && r->applied; }
int xideResourceRebindOpen(xide_resource_rebind* p,xide_model_error* e)
{
 if(!xideResourceRebindValidate(p,e)) return 0;
 for(size_t i=0;i<p->count;++i) { resource_row* r=&p->rows[i]; if(!r->affected) continue;
  if(!r->document) { r->document=xideDocumentOpen(p->documents,r->path,e); if(!r->document) return 0;
   r->opened=1; r->id=r->document->id; r->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(r->document->editor)); }
  if(strcmp(xuiCodeEditGetText(r->document->editor),r->before)) return fail(e,XIDE_MODEL_CONFLICT,"资源配置在预打开时发生变化。");
  xui_code_selection_model selection=xuiCodeEditGetSelection(r->document->editor); r->selectionCount=xuiCodeSelectionGetCount(selection);
  free(r->selections); r->selections=r->selectionCount ? calloc((size_t)r->selectionCount,sizeof(*r->selections)) : NULL;
  if(r->selectionCount && !r->selections) return fail(e,XIDE_MODEL_MEMORY,"无法准备资源文档阅读位置。");
  for(int j=0;j<r->selectionCount;++j) { saved_position* s=&r->selections[j];
   if(xuiCodeSelectionGetAt(selection,j,&s->state)!=XUI_OK) return fail(e,XIDE_MODEL_INVALID,"无法读取资源文档选区。");
   xidePositionFromOffset(r->before,s->state.iAnchorOffset,&s->anchorLine,&s->anchorColumn); xidePositionFromOffset(r->before,s->state.iCaretOffset,&s->caretLine,&s->caretColumn);
  }
  xuiCodeEditGetScroll(r->document->editor,&r->scrollX,&r->scrollY);
 }
 return xideResourceRebindValidate(p,e);
}
void xideResourceRebindRollbackOpen(xide_resource_rebind* p)
{
 if(p) for(size_t i=0;i<p->count;++i) { resource_row* r=&p->rows[i]; if(r->opened && !r->applied) { xideDocumentClose(r->document,1,NULL); r->document=NULL; r->opened=0; } }
}
int xideResourceRebindApply(xide_resource_rebind* p,xide_model_error* e)
{
 for(size_t i=0;i<p->count;++i) { resource_row* r=&p->rows[i]; if(!r->affected || r->applied) continue;
  xide_document* d=r->document;
  if(!d || !d->open || d->id!=r->id || strcmp(xuiCodeEditGetText(d->editor),r->before)) return fail(e,XIDE_MODEL_CONFLICT,"文件已改名，待更新资源文档已改变；请恢复正文后重试完成资源更新。");
  xui_code_document core=xuiCodeEditGetDocument(d->editor);
  if(!r->after || (strcmp(r->before,r->after) && xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),r->after)!=XUI_OK))
   return fail(e,XIDE_MODEL_MEMORY,"文件已改名，资源正文尚未全部更新。请重试“完成资源更新”；不要关闭程序。");
  xui_code_selection_model selection=xuiCodeEditGetSelection(d->editor); xuiCodeSelectionClear(selection);
  for(int j=0;j<r->selectionCount;++j) { xui_code_selection_t state=r->selections[j].state;
   state.iAnchorOffset=xidePositionToOffset(r->after,r->selections[j].anchorLine,r->selections[j].anchorColumn);
   state.iCaretOffset=xidePositionToOffset(r->after,r->selections[j].caretLine,r->selections[j].caretColumn);
   if(!j) xuiCodeSelectionSetState(selection,core,&state); else xuiCodeSelectionAdd(selection,&state);
  }
  xuiCodeEditSetScroll(d->editor,r->scrollX,r->scrollY);
  r->applied=1;
 }
 return 1;
}
void xideResourceRebindDestroy(xide_resource_rebind* p)
{ if(p) { for(size_t i=0;i<p->count;++i) { free(p->rows[i].path); free(p->rows[i].before); free(p->rows[i].selections); xrtFree(p->rows[i].after); } free(p->rows); free(p->source); free(p); } }
