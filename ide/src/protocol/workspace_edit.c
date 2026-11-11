#include "protocol/workspace_edit.h"
#include "services/reference_document.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef XIDE_WORKSPACE_EDIT_ALLOC
#define XIDE_WORKSPACE_EDIT_ALLOC xrtCalloc
#endif
#define EDIT_FILE_BYTES (8u*1024u*1024u)
#define EDIT_TOTAL_BYTES (64u*1024u*1024u)
#define EDIT_MAX_BYTES (256u*1024u*1024u)
typedef struct capture {const xide_workspace_edit_input* input;size_t index;char* path;wchar_t* key;int used;} capture;
typedef struct pending_edit {xide_workspace_text_edit view;xstrview replacement;} pending_edit;
typedef struct preparation {int (*cancelled)(void*);void* user;int stopped;size_t used,limit;} preparation;
struct xide_workspace_edit {xatomic32 references;xide_workspace_edit_info info;xarray documents,edits,annotations;};
static const xvalue* field(const xvalue* v,const char* k){return xideReferenceDocumentField(v,k);}
static int stop(preparation* p){if(!p->stopped && p->cancelled && p->cancelled(p->user))p->stopped=1;return p->stopped;}
static int cancelledPreparation(void* user){return stop(user);}
static int charge(preparation* p,size_t n){if(n>p->limit-p->used)return 0;p->used+=n;return 1;}
static int stringView(const xvalue* value,size_t limit,int empty,xstrview* out)
{return value && xrtValueGetString(value,out) && (empty || out->Size) && out->Size<=limit && !memchr(out->Data,0,out->Size) && xrtUtf8Valid(*out,NULL);}
static int integer(const xvalue* value,int* out)
{int64 n=0;if(!value || !xrtValueGetInt(value,&n) || n<INT_MIN || n>INT_MAX)return 0;*out=(int)n;return 1;}
static int position(const xvalue* value,xide_lsp_position* out)
{return integer(field(value,"line"),&out->line) && out->line>=0 && integer(field(value,"character"),&out->character) && out->character>=0;}
static char* copyString(xstrview v,preparation* p)
{if(!charge(p,v.Size+1))return NULL;char* s=XIDE_WORKSPACE_EDIT_ALLOC(1,v.Size+1);if(s && v.Size)memcpy(s,v.Data,v.Size);return s;}
static int array(xarray* a,size_t count,size_t size,preparation* p)
{if(!xrtArrayInit(a,size) || count>(p->limit-p->used)/size || !xrtArrayResize(a,count))return 0;if(count)memset(a->Data,0,count*size);if(a->Capacity>(p->limit-p->used)/size)return 0;return charge(p,a->Capacity*size);}
static int captureOrder(const void* a,const void* b){return xidePathNativeKeyCompare(((const capture*)a)->key,((const capture*)b)->key);}
static int editOrder(const void* a,const void* b)
{const pending_edit *x=a,*y=b;if(x->view.start.offset!=y->view.start.offset)return x->view.start.offset<y->view.start.offset?-1:1;return (x->view.ordinal>y->view.ordinal)-(x->view.ordinal<y->view.ordinal);}
static int annotationOrder(const void* a,const void* b)
{return strcmp(((const xide_workspace_edit_annotation*)a)->id.Data,((const xide_workspace_edit_annotation*)b)->id.Data);}
static size_t annotationFind(const xide_workspace_edit* p,const char* id)
{size_t a=0,b=p->annotations.Count;while(a<b){size_t m=a+(b-a)/2;const xide_workspace_edit_annotation* row=xrtArrayConstGet(&p->annotations,m);if(strcmp(row->id.Data,id)<0)a=m+1;else b=m;}const xide_workspace_edit_annotation* row=xrtArrayConstGet(&p->annotations,a);return row && !strcmp(row->id.Data,id)?a:SIZE_MAX;}
static int hashText(const xls_text* text,unsigned char hash[32],preparation* p)
{xsha256 state;xrtSha256Init(&state);size_t n=xlsTextSize(text);const char* s=xlsTextData(text);for(size_t at=0;at<n;){if(stop(p))return 0;size_t step=n-at>65536?65536:n-at;if(!xrtSha256Update(&state,s+at,step))return 0;at+=step;}return xrtSha256Final(&state,hash);}
static void decodeHash(const char* s,unsigned char* out)
{for(size_t i=0;i<32;++i){unsigned a=(unsigned char)s[2*i],b=(unsigned char)s[2*i+1];out[i]=(unsigned char)(((a<='9'?a-'0':a-'a'+10)<<4)|(b<='9'?b-'0':b-'a'+10));}}
static int proof(const xvalue* row,int required,xide_workspace_edit_document* d,preparation* p)
{
 const xvalue* extension=field(row,"xlang");if(!extension)return !required;
 int schema=0;const xvalue* value=field(extension,"document");if(!integer(field(extension,"schema"),&schema) || schema!=1 || !xideReferenceDocumentValid(value,d->path))return 0;
 xide_workspace_edit_proof* out=&d->proof;int size=0;integer(field(value,"normalizedSize"),&size);out->normalizedSize=(size_t)size;
 if(out->normalizedSize!=xlsTextSize(d->before))return 0;
 decodeHash(xideReferenceDocumentString(field(value,"normalizedSha256"),64),out->normalizedSha256);unsigned char actual[32];if(!hashText(d->before,actual,p) || memcmp(actual,out->normalizedSha256,32))return 0;
 const char* origin=xideReferenceDocumentString(field(value,"origin"),16);out->overlay=!strcmp(origin,"overlay");
 if(out->overlay){if(!d->hasVersion || !integer(field(value,"version"),&out->version) || out->version!=d->version)return 0;}
 else {if(d->hasVersion)return 0;const xvalue* input=field(value,"input");integer(field(input,"rawSize"),&size);out->rawSize=(size_t)size;decodeHash(xideReferenceDocumentString(field(input,"rawSha256"),64),out->rawSha256);}
 out->present=1;return 1;
}
xide_workspace_edit* xideWorkspaceEditRetain(xide_workspace_edit* p){if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xideWorkspaceEditRelease(xide_workspace_edit* p)
{
 if(!p || xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;i<p->documents.Count;++i){xide_workspace_edit_document* d=xrtArrayGet(&p->documents,i);xrtFree((char*)d->uri);free((char*)d->path);xlsTextRelease(d->before);xlsTextRelease(d->after);}
 for(size_t i=0;i<p->annotations.Count;++i){xide_workspace_edit_annotation* a=xrtArrayGet(&p->annotations,i);xrtFree((char*)a->id.Data);xrtFree((char*)a->label.Data);xrtFree((char*)a->description.Data);}
 xrtArrayUnit(&p->annotations);xrtArrayUnit(&p->edits);xrtArrayUnit(&p->documents);xrtFree(p);
}
int xideWorkspaceEditInfo(const xide_workspace_edit* p,xide_workspace_edit_info* out){if(out)*out=(xide_workspace_edit_info){0};if(!p || !out)return 0;*out=p->info;return 1;}
int xideWorkspaceEditDocumentAt(const xide_workspace_edit* p,size_t n,xide_workspace_edit_document* out){if(out)*out=(xide_workspace_edit_document){0};if(!p || !out || n>=p->documents.Count)return 0;*out=*(const xide_workspace_edit_document*)xrtArrayConstGet(&p->documents,n);return 1;}
int xideWorkspaceEditAt(const xide_workspace_edit* p,size_t n,xide_workspace_text_edit* out){if(out)*out=(xide_workspace_text_edit){0};if(!p || !out || n>=p->edits.Count)return 0;*out=*(const xide_workspace_text_edit*)xrtArrayConstGet(&p->edits,n);return 1;}
int xideWorkspaceEditAnnotationAt(const xide_workspace_edit* p,size_t n,xide_workspace_edit_annotation* out){if(out)*out=(xide_workspace_edit_annotation){0};if(!p || !out || n>=p->annotations.Count)return 0;*out=*(const xide_workspace_edit_annotation*)xrtArrayConstGet(&p->annotations,n);return 1;}
int xideWorkspaceEditStillCurrent(const xide_workspace_edit* p,size_t at,const xide_workspace_edit_input* in)
{
 const xide_workspace_edit_document* d=p?xrtArrayConstGet(&p->documents,at):NULL;
 return d && in && in->uri && in->text==d->before && in->identity==d->identity && in->revision==d->revision && in->hasVersion==d->hasVersion && (!in->hasVersion || in->version==d->version) && !strcmp(in->uri,d->uri);
}
xide_workspace_edit* xideWorkspaceEditSelect(const xide_workspace_edit* source,const size_t* selected,size_t count,size_t budget,int (*cancelled)(void*),void* user,enum xide_workspace_edit_error* output)
{
 preparation p={cancelled,user,0,0,budget?budget:128u*1024u*1024u};enum xide_workspace_edit_error why=XIDE_WORKSPACE_EDIT_ARGUMENT;xide_workspace_edit* plan=NULL;unsigned char used[4096]={0};size_t annotationMap[4096],edits=0,annotations=0;
 if(!source||(count&&!selected)||count>source->info.documents||count>512||source->info.annotations>4096)goto done;
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(p.limit>EDIT_MAX_BYTES||!charge(&p,sizeof(*plan)))goto done;
 for(size_t i=0;i<count;++i){why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;why=XIDE_WORKSPACE_EDIT_ARGUMENT;if(selected[i]>=source->info.documents||(i&&selected[i]<=selected[i-1]))goto done;const xide_workspace_edit_document* d=xrtArrayConstGet(&source->documents,selected[i]);if(!d||d->count>65536-edits)goto done;edits+=d->count;
  for(size_t j=0;j<d->count;++j){if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}const xide_workspace_text_edit* e=xrtArrayConstGet(&source->edits,d->first+j);if(!e||e->document!=selected[i]||(e->annotation!=SIZE_MAX&&e->annotation>=source->info.annotations))goto done;if(e->annotation!=SIZE_MAX)used[e->annotation]=1;}
 }
 for(size_t i=0;i<source->info.annotations;++i)annotationMap[i]=used[i]?annotations++:SIZE_MAX;
 why=XIDE_WORKSPACE_EDIT_MEMORY;plan=XIDE_WORKSPACE_EDIT_ALLOC(1,sizeof(*plan));if(!plan)goto done;xrtAtomic32Init(&plan->references,1);
 if(!array(&plan->documents,count,sizeof(xide_workspace_edit_document),&p)||!array(&plan->edits,edits,sizeof(xide_workspace_text_edit),&p)||!array(&plan->annotations,annotations,sizeof(xide_workspace_edit_annotation),&p))goto done;
 size_t at=0;
 for(size_t i=0;i<count;++i){why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;const xide_workspace_edit_document* original=xrtArrayConstGet(&source->documents,selected[i]);xide_workspace_edit_document* d=xrtArrayGet(&plan->documents,i);*d=*original;d->uri=d->path=NULL;d->before=d->after=NULL;d->first=at;
  why=XIDE_WORKSPACE_EDIT_LIMIT;size_t pathBytes=strlen(original->path)+1,before=xlsTextStorageBytes(original->before),after=original->after==original->before?0:xlsTextStorageBytes(original->after);if(!before||!charge(&p,before)||!charge(&p,after)||!charge(&p,pathBytes))goto done;
  why=XIDE_WORKSPACE_EDIT_MEMORY;d->uri=copyString(xrtStrView(original->uri),&p);char* path=malloc(pathBytes);d->path=path;if(!d->uri||!path)goto done;memcpy(path,original->path,pathBytes);d->before=xlsTextRetain(original->before);d->after=xlsTextRetain(original->after);if(!d->before||!d->after)goto done;plan->info.changedDocuments+=d->changed;
  for(size_t j=0;j<d->count;++j){why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;xide_workspace_text_edit* e=xrtArrayGet(&plan->edits,at++);*e=*(const xide_workspace_text_edit*)xrtArrayConstGet(&source->edits,original->first+j);e->document=i;if(e->annotation!=SIZE_MAX)e->annotation=annotationMap[e->annotation];}
 }
 for(size_t i=0;i<source->info.annotations;++i)if(used[i]){why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;const xide_workspace_edit_annotation* a=xrtArrayConstGet(&source->annotations,i);xide_workspace_edit_annotation* b=xrtArrayGet(&plan->annotations,annotationMap[i]);b->needsConfirmation=a->needsConfirmation;plan->info.needsConfirmation|=b->needsConfirmation;why=XIDE_WORKSPACE_EDIT_MEMORY;b->id=(xstrview){copyString(a->id,&p),a->id.Size};b->label=(xstrview){copyString(a->label,&p),a->label.Size};if(a->description.Data)b->description=(xstrview){copyString(a->description,&p),a->description.Size};if(!b->id.Data||!b->label.Data||(a->description.Data&&!b->description.Data))goto done;}
 why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;plan->info.documents=count;plan->info.edits=edits;plan->info.annotations=annotations;plan->info.bytes=p.used;why=XIDE_WORKSPACE_EDIT_OK;
done:
 if(p.stopped)why=XIDE_WORKSPACE_EDIT_CANCELLED;
 if(why!=XIDE_WORKSPACE_EDIT_OK){xideWorkspaceEditRelease(plan);plan=NULL;}if(output)*output=why;return plan;
}
xide_workspace_edit* xideWorkspaceEditPrepare(const xvalue* value,const xide_workspace_edit_input* inputs,size_t count,int requireProof,size_t budget,int (*cancelled)(void*),void* user,enum xide_workspace_edit_error* output)
{
 enum xide_workspace_edit_error why=XIDE_WORKSPACE_EDIT_ARGUMENT;xide_workspace_edit* plan=NULL;capture* captured=NULL;pending_edit* pending=NULL;xide_lsp_position* positions=NULL;char* body=NULL;wchar_t* lookup=NULL;
 preparation p={cancelled,user,0,0,budget?budget:128u*1024u*1024u};size_t docs=0,edits=0,annotations=0,prepared=0,uriBytes=0,beforeBytes=0,afterBytes=0,scratchBytes=0,replacementBytes=0;
 if(!value || xrtValueType(value)!=XVALUE_OBJECT || (count && !inputs) || count>512 || (requireProof!=0 && requireProof!=1))goto done;
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(p.limit>EDIT_MAX_BYTES || !charge(&p,sizeof(*plan)))goto done;
 why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;
 const xvalue *changes=field(value,"documentChanges"),*notes=field(value,"changeAnnotations");int versioned=changes!=NULL;
 if(!changes)changes=field(value,"changes");
 why=XIDE_WORKSPACE_EDIT_SHAPE;if(changes && xrtValueType(changes)!=(versioned?XVALUE_ARRAY:XVALUE_OBJECT))goto done;
 if(notes && xrtValueType(notes)!=XVALUE_OBJECT)goto done;
 docs=changes?xrtValueCount(changes):0;annotations=notes?xrtValueCount(notes):0;
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(docs>512 || annotations>4096 || count>SIZE_MAX/sizeof(*captured) || !charge(&p,count*sizeof(*captured)))goto done;scratchBytes=count*sizeof(*captured);
 why=XIDE_WORKSPACE_EDIT_MEMORY;captured=XIDE_WORKSPACE_EDIT_ALLOC(count?count:1,sizeof(*captured));plan=XIDE_WORKSPACE_EDIT_ALLOC(1,sizeof(*plan));if(plan)xrtAtomic32Init(&plan->references,1);if(!captured || !plan)goto done;
 for(size_t i=0;i<count;++i){
  if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}const xide_workspace_edit_input* in=inputs+i;captured[i].input=in;captured[i].index=i;prepared=i+1;
  why=XIDE_WORKSPACE_EDIT_ARGUMENT;if(!in->uri || !in->text || (in->hasVersion!=0 && in->hasVersion!=1) || (in->hasVersion && !in->identity))goto done;
  size_t bytes=strlen(in->uri)+1;why=XIDE_WORKSPACE_EDIT_LIMIT;if(bytes>65536 || bytes>1024u*1024u-uriBytes || !charge(&p,bytes))goto done;uriBytes+=bytes;scratchBytes+=bytes;
  why=XIDE_WORKSPACE_EDIT_SOURCE;captured[i].path=xideLspUriToPath(in->uri,NULL);if(!captured[i].path)goto done;captured[i].key=xidePathLexicalWide(captured[i].path);if(!captured[i].key)goto done;
  bytes=strlen(captured[i].path)+1+(wcslen(captured[i].key)+1)*sizeof(wchar_t);why=XIDE_WORKSPACE_EDIT_LIMIT;if(!charge(&p,bytes))goto done;scratchBytes+=bytes;
 }
 if(count)qsort(captured,count,sizeof(*captured),captureOrder);
 why=XIDE_WORKSPACE_EDIT_SOURCE;for(size_t i=1;i<count;++i)if(!captureOrder(captured+i-1,captured+i))goto done;
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(!array(&plan->documents,docs,sizeof(xide_workspace_edit_document),&p) || !array(&plan->annotations,annotations,sizeof(xide_workspace_edit_annotation),&p))goto done;
 for(size_t i=0;i<annotations;++i){
  if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}xstrview key={0},label={0},description={0};const xvalue* note=xrtValueObjectAt(notes,i,&key);xide_workspace_edit_annotation* a=xrtArrayGet(&plan->annotations,i);
  why=XIDE_WORKSPACE_EDIT_SHAPE;if(key.Size>65535 || memchr(key.Data,0,key.Size) || !xrtUtf8Valid(key,NULL) || !stringView(field(note,"label"),65535,1,&label))goto done;
  const xvalue* desc=field(note,"description");if(desc && !stringView(desc,65535,1,&description))goto done;
  const xvalue* confirmation=field(note,"needsConfirmation");bool confirm=false;if(confirmation && !xrtValueGetBool(confirmation,&confirm))goto done;
  a->needsConfirmation=confirm;a->id=(xstrview){copyString(key,&p),key.Size};a->label=(xstrview){copyString(label,&p),label.Size};if(desc)a->description=(xstrview){copyString(description,&p),description.Size};
  why=XIDE_WORKSPACE_EDIT_MEMORY;if(!a->id.Data || !a->label.Data || (desc && !a->description.Data))goto done;
 }
 if(annotations)qsort(plan->annotations.Data,annotations,sizeof(xide_workspace_edit_annotation),annotationOrder);
 /* First pass binds documents and validates versions/proofs before building any
  * replacement body. The original DOM is borrowed only for this constructor. */
 for(size_t i=0;i<docs;++i){
  if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}xstrview uri={0};const xvalue *row=versioned?xrtValueArrayGet(changes,i):NULL,*identifier=field(row,"textDocument"),*items=NULL;
  why=XIDE_WORKSPACE_EDIT_UNSUPPORTED;if(versioned && field(row,"kind"))goto done;
  why=XIDE_WORKSPACE_EDIT_SHAPE;if(versioned){if(!stringView(field(identifier,"uri"),65535,0,&uri))goto done;items=field(row,"edits");}else items=xrtValueObjectAt(changes,i,&uri);
  if(!items || xrtValueType(items)!=XVALUE_ARRAY || !uri.Size || uri.Size>65535 || memchr(uri.Data,0,uri.Size) || !xrtUtf8Valid(uri,NULL))goto done;
  size_t n=xrtValueCount(items);why=XIDE_WORKSPACE_EDIT_LIMIT;if(n>65536-edits)goto done;edits+=n;
  why=XIDE_WORKSPACE_EDIT_SOURCE;char* path=xideLspUriToPath(uri.Data,NULL);if(!path)goto done;lookup=xidePathLexicalWide(path);free(path);if(!lookup)goto done;
  size_t low=0,high=count;while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(captured[mid].key,lookup)<0)low=mid+1;else high=mid;}if(low==count || xidePathNativeKeyCompare(captured[low].key,lookup) || captured[low].used)goto done;free(lookup);lookup=NULL;
  capture* c=captured+low;const xide_workspace_edit_input* in=c->input;c->used=1;xide_workspace_edit_document* d=xrtArrayGet(&plan->documents,i);
  d->path=c->path;scratchBytes-=strlen(c->path)+1;c->path=NULL;d->uri=copyString(xrtStrView(in->uri),&p);why=XIDE_WORKSPACE_EDIT_MEMORY;if(!d->uri)goto done;
  d->input=c->index;d->identity=in->identity;d->revision=in->revision;d->hasVersion=in->hasVersion;d->version=in->version;d->before=xlsTextRetain(in->text);d->count=n;d->first=edits-n;
  why=XIDE_WORKSPACE_EDIT_VERSION;const xvalue* version=versioned?field(identifier,"version"):NULL;int expected=0;if(versioned && !version)goto done;if(version && xrtValueType(version)!=XVALUE_NULL && (!integer(version,&expected) || !in->hasVersion || expected!=in->version))goto done;
  size_t length=xlsTextSize(in->text),storage=xlsTextStorageBytes(in->text);why=XIDE_WORKSPACE_EDIT_LIMIT;if(length>EDIT_FILE_BYTES || length>EDIT_TOTAL_BYTES-beforeBytes || !storage || !charge(&p,storage))goto done;beforeBytes+=length;
  const char* text=xlsTextData(in->text);why=XIDE_WORKSPACE_EDIT_SOURCE;if(memchr(text,'\r',length) || (length>=3 && !memcmp(text,"\xef\xbb\xbf",3)))goto done;
  why=XIDE_WORKSPACE_EDIT_PROOF;if(!proof(row,requireProof,d,&p))goto done;
 }
 why=XIDE_WORKSPACE_EDIT_LIMIT;if(!array(&plan->edits,edits,sizeof(xide_workspace_text_edit),&p) || edits>(p.limit-p.used)/(sizeof(*pending)+2*sizeof(*positions)))goto done;
 size_t editScratch=edits*(sizeof(*pending)+2*sizeof(*positions));if(!charge(&p,editScratch))goto done;scratchBytes+=editScratch;
 why=XIDE_WORKSPACE_EDIT_MEMORY;pending=XIDE_WORKSPACE_EDIT_ALLOC(edits?edits:1,sizeof(*pending));positions=XIDE_WORKSPACE_EDIT_ALLOC(edits?2*edits:1,sizeof(*positions));if(!pending || !positions)goto done;
 for(size_t i=0;i<docs;++i){
  xide_workspace_edit_document* d=xrtArrayGet(&plan->documents,i);const xvalue* row=versioned?xrtValueArrayGet(changes,i):NULL;const xvalue* items=versioned?field(row,"edits"):xrtValueObjectAt(changes,i,NULL);size_t length=xlsTextSize(d->before),after=length;
  if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}
  if(!d->count){why=XIDE_WORKSPACE_EDIT_LIMIT;if(length>EDIT_TOTAL_BYTES-afterBytes)goto done;afterBytes+=length;d->after=xlsTextRetain(d->before);continue;}
  for(size_t j=0;j<d->count;++j){
   if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}pending_edit* e=pending+j;*e=(pending_edit){.view={.document=i,.ordinal=j,.annotation=SIZE_MAX}};const xvalue* change=xrtValueArrayGet(items,j),*range=field(change,"range");
   why=XIDE_WORKSPACE_EDIT_UNSUPPORTED;if(field(change,"insertTextFormat") || field(change,"snippet"))goto done;
   why=XIDE_WORKSPACE_EDIT_SHAPE;if(!position(field(range,"start"),&e->view.start) || !position(field(range,"end"),&e->view.end) || !xrtValueGetString(field(change,"newText"),&e->replacement))goto done;
   why=XIDE_WORKSPACE_EDIT_LIMIT;if(e->replacement.Size>XIDE_LSP_BODY_BYTES-replacementBytes)goto done;replacementBytes+=e->replacement.Size;
   why=XIDE_WORKSPACE_EDIT_SHAPE;if(!stringView(field(change,"newText"),EDIT_FILE_BYTES,1,&e->replacement))goto done;
   const xvalue* annotation=field(change,"annotationId");if(annotation){xstrview id={0};if(!versioned || !stringView(annotation,65535,1,&id) || (e->view.annotation=annotationFind(plan,id.Data))==SIZE_MAX)goto done;const xide_workspace_edit_annotation* a=xrtArrayConstGet(&plan->annotations,e->view.annotation);plan->info.needsConfirmation|=a->needsConfirmation;}
   positions[2*j]=e->view.start;positions[2*j+1]=e->view.end;
  }
  why=XIDE_WORKSPACE_EDIT_RANGE;if(!xideLspManyPositionsToOffsets(xlsTextData(d->before),positions,2*d->count,cancelledPreparation,&p))goto done;
  for(size_t j=0;j<d->count;++j){pending[j].view.start=positions[2*j];pending[j].view.end=positions[2*j+1];if(pending[j].view.end.offset<pending[j].view.start.offset)goto done;}
  if(d->count)qsort(pending,d->count,sizeof(*pending),editOrder);
  size_t previous=0,removed=0,inserted=0;
  for(size_t j=0;j<d->count;++j){pending_edit* e=pending+j;why=XIDE_WORKSPACE_EDIT_OVERLAP;if((size_t)e->view.start.offset<previous)goto done;previous=(size_t)e->view.end.offset;removed+=(size_t)(e->view.end.offset-e->view.start.offset);size_t added=0;
   for(size_t k=0;k<e->replacement.Size;++k){if(!(k%65536) && stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}++added;if(e->replacement.Data[k]=='\r' && k+1<e->replacement.Size && e->replacement.Data[k+1]=='\n')++k;}
   why=XIDE_WORKSPACE_EDIT_LIMIT;if(added>EDIT_FILE_BYTES-inserted)goto done;inserted+=added;
  }
  why=XIDE_WORKSPACE_EDIT_LIMIT;if(removed>length || inserted>EDIT_FILE_BYTES-(length-removed))goto done;after=length-removed+inserted;
  why=XIDE_WORKSPACE_EDIT_LIMIT;if(after>EDIT_FILE_BYTES || after>EDIT_TOTAL_BYTES-afterBytes || after+1>p.limit-p.used)goto done;afterBytes+=after;
  why=XIDE_WORKSPACE_EDIT_MEMORY;body=XIDE_WORKSPACE_EDIT_ALLOC(after+1,1);if(!body)goto done;size_t at=0;previous=0;const char* before=xlsTextData(d->before);
  for(size_t j=0;j<=d->count;++j){pending_edit* e=j<d->count?pending+j:NULL;size_t end=e?(size_t)e->view.start.offset:length;
   while(previous<end){if(stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}size_t block=end-previous>65536?65536:end-previous;memcpy(body+at,before+previous,block);at+=block;previous+=block;}
   if(!e){break;}e->view.afterStart=at;
   for(size_t k=0;k<e->replacement.Size;++k){if(!(k%65536) && stop(&p)){why=XIDE_WORKSPACE_EDIT_CANCELLED;goto done;}char ch=e->replacement.Data[k];if(ch=='\r'){ch='\n';if(k+1<e->replacement.Size && e->replacement.Data[k+1]=='\n')++k;}body[at++]=ch;}
   e->view.afterEnd=at;previous=(size_t)e->view.end.offset;*(xide_workspace_text_edit*)xrtArrayGet(&plan->edits,d->first+j)=e->view;
  }
  if(at!=after){why=XIDE_WORKSPACE_EDIT_RANGE;goto done;}d->changed=after!=length || memcmp(body,before,length);
  if(!d->changed)d->after=xlsTextRetain(d->before);else{
   enum xls_text_error error;d->after=xlsTextCreate(body,after,EDIT_FILE_BYTES,&error);why=error==XLS_TEXT_MEMORY?XIDE_WORKSPACE_EDIT_MEMORY:XIDE_WORKSPACE_EDIT_LIMIT;if(!d->after)goto done;
   size_t storage=xlsTextStorageBytes(d->after);if(!storage || storage>p.limit-p.used-(after+1))goto done;if(!charge(&p,storage))goto done;
  }
  xrtFree(body);body=NULL;plan->info.changedDocuments+=d->changed;
 }
 why=XIDE_WORKSPACE_EDIT_CANCELLED;if(stop(&p))goto done;
 plan->info.documents=docs;plan->info.edits=edits;plan->info.annotations=annotations;plan->info.bytes=p.used-scratchBytes;why=XIDE_WORKSPACE_EDIT_OK;
done:
 if(p.stopped){why=XIDE_WORKSPACE_EDIT_CANCELLED;}free(lookup);xrtFree(body);xrtFree(pending);xrtFree(positions);
 for(size_t i=0;i<prepared;++i){free(captured[i].path);free(captured[i].key);}xrtFree(captured);
 if(why!=XIDE_WORKSPACE_EDIT_OK){xideWorkspaceEditRelease(plan);plan=NULL;}if(output)*output=why;return plan;
}
