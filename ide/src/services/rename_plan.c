#include "services/rename_plan.h"
#ifdef XLS_COMPILER_XRT
#include "services/reference_document.h"
#include "analysis/identifier.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "src/lexer/xl6_literal_span.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#ifndef XIDE_RENAME_PLAN_ALLOC
#define XIDE_RENAME_PLAN_ALLOC xrtMalloc
#endif
#define PLAN_BYTES (128u*1024u*1024u)
#define PLAN_ITEMS 65536u
typedef struct plan_stop {int (*cancelled)(void*);void* user;int latched;} plan_stop;
typedef struct plan_document {xide_rename_plan_document view;size_t beforeSize,afterSize;} plan_document;
typedef struct plan_pending {const xide_rename_plan_input* input;wchar_t* key;char* uri;size_t original,beforeSize,afterSize,first,count;int bom,versioned,version;} plan_pending;
struct xide_rename_plan {xatomic32 references;xide_rename_plan_info info;plan_document* documents;xide_rename_plan_edit* edits;xstrview replacement;};
static int planStopped(plan_stop* s){if(!s->latched && s->cancelled)s->latched=s->cancelled(s->user)!=0;return s->latched;}
static int planCancel(void* user){return planStopped(user);}
static int planCharge(size_t* used,size_t add,size_t limit){if(*used>limit || add>limit-*used)return 0;*used+=add;return 1;}
static int planCopy(char* target,const char* source,size_t bytes,plan_stop* stop)
{while(bytes){if(planStopped(stop))return 0;size_t n=bytes>65536?65536:bytes;memcpy(target,source,n);target+=n;source+=n;bytes-=n;}return 1;}
static int planDocOrder(const void* a,const void* b){return xidePathNativeKeyCompare(((const plan_pending*)a)->key,((const plan_pending*)b)->key);}
static int planEditOrder(const void* a,const void* b)
{const xide_rename_plan_edit *x=a,*y=b;int n=(x->document>y->document)-(x->document<y->document);if(!n)n=(x->start.offset>y->start.offset)-(x->start.offset<y->start.offset);return n ? n : (x->end.offset>y->end.offset)-(x->end.offset<y->end.offset);}
static int planDigestEquals(const unsigned char* digest,const xvalue* value)
{char hex[65];for(size_t i=0;i<32;++i){static const char digits[]="0123456789abcdef";hex[2*i]=digits[digest[i]>>4];hex[2*i+1]=digits[digest[i]&15];}hex[64]=0;const char* expected=xideReferenceDocumentString(value,64);return expected && !strcmp(hex,expected);}
/* Same LF normalization as the source-proof producer, streamed through XRT. */
static int planNormalized(const char* text,size_t bytes,int prefix,const xvalue* proof,plan_stop* stop)
{
 xsha256 hash;xrtSha256Init(&hash);size_t length=prefix ? 3 : 0,used=0;char buffer[16384];
 if(prefix && !xrtSha256Update(&hash,"\xef\xbb\xbf",3))return 0;
 for(size_t i=0;i<bytes;++i){if(!(i%16384) && planStopped(stop))return 0;char c=text[i];if(c=='\r'){if(i+1<bytes && text[i+1]=='\n')++i;c='\n';}buffer[used++]=c;++length;if(used==sizeof(buffer)){if(!xrtSha256Update(&hash,buffer,used))return 0;used=0;}}
 unsigned char digest[32];int size=0;return (!used || xrtSha256Update(&hash,buffer,used)) && xrtSha256Final(&hash,digest) && xideReferenceDocumentInteger(xideReferenceDocumentField(proof,"normalizedSize"),&size) && length==(size_t)size && planDigestEquals(digest,xideReferenceDocumentField(proof,"normalizedSha256"));
}
static int planSource(plan_pending* pending,plan_stop* stop)
{
 const xide_semantic_source* source=&pending->input->source;const xvalue* proof=pending->input->document;
 if(!source->path || !source->text || !(source->kind&1) || (source->utf16Prefix!=0 && source->utf16Prefix!=1) || !xideReferenceDocumentValid(proof,source->path))return 0;
 pending->beforeSize=strlen(source->text);if(pending->beforeSize>8u*1024u*1024u || !xrtUtf8Valid((xstrview){source->text,pending->beforeSize},NULL))return 0;
 const char* origin=xideReferenceDocumentString(xideReferenceDocumentField(proof,"origin"),16);pending->versioned=!strcmp(origin,"overlay");
 if(pending->versioned){int64 version=0;if(!xrtValueGetInt(xideReferenceDocumentField(proof,"version"),&version) || version<INT_MIN || version>INT_MAX)return 0;pending->version=(int)version;pending->bom=source->utf16Prefix;}
 else{
  if(source->utf16Prefix)return 0;
  const xvalue* input=xideReferenceDocumentField(proof,"input");int rawSize=0;if(!xideReferenceDocumentInteger(xideReferenceDocumentField(input,"rawSize"),&rawSize))return 0;
  if((size_t)rawSize==pending->beforeSize)pending->bom=0;else if((size_t)rawSize==pending->beforeSize+3)pending->bom=1;else return 0;
  xsha256 hash;xrtSha256Init(&hash);if(pending->bom && !xrtSha256Update(&hash,"\xef\xbb\xbf",3))return 0;
  for(size_t at=0;at<pending->beforeSize;){if(planStopped(stop))return 0;size_t n=pending->beforeSize-at;if(n>65536)n=65536;if(!xrtSha256Update(&hash,source->text+at,n))return 0;at+=n;}
  unsigned char digest[32];if(!xrtSha256Final(&hash,digest) || !planDigestEquals(digest,xideReferenceDocumentField(input,"rawSha256")))return 0;
 }
 return planNormalized(source->text,pending->beforeSize,pending->versioned && pending->bom,proof,stop);
}
typedef struct plan_count {size_t bytes,limit;plan_stop* stop;} plan_count;
static bool planCount(xbytesview bytes,void* user){plan_count* c=user;return !planStopped(c->stop) && planCharge(&c->bytes,bytes.Size,c->limit);}
xide_rename_plan* xideRenamePlanRetain(xide_rename_plan* p){if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xideRenamePlanRelease(xide_rename_plan* p)
{if(p && xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)==1){for(size_t i=0;i<p->info.documents;++i)xrtValueRelease((xvalue*)p->documents[i].view.proof);xrtFree(p);}}
xide_rename_plan* xideRenamePlanCreate(const xide_rename_plan_input* inputs,size_t documents,const xide_rename_plan_change* changes,size_t edits,
 xstrview oldName,xstrview newName,size_t limit,int (*cancelled)(void*),void* user,enum xide_rename_plan_error* output)
{
 enum xide_rename_plan_error why=XIDE_RENAME_PLAN_ARGUMENT;plan_stop stop={cancelled,user,0};xide_rename_plan* plan=NULL;void* scratch=NULL;plan_pending* pending=NULL;size_t prepared=0;
 if(!limit)limit=PLAN_BYTES;
 if(!inputs || !documents || !changes || !edits)goto done;
 why=XIDE_RENAME_PLAN_NAME;if(!xlsIdentifierValid(oldName.Data,oldName.Size) || !xlsIdentifierValid(newName.Data,newName.Size))goto done;
 why=XIDE_RENAME_PLAN_UNCHANGED;if(oldName.Size==newName.Size && !memcmp(oldName.Data,newName.Data,newName.Size))goto done;
 why=XIDE_RENAME_PLAN_LIMIT;if(limit>PLAN_BYTES || documents>PLAN_ITEMS || edits>PLAN_ITEMS || documents>edits)goto done;
 if(planStopped(&stop))goto done;
 size_t known=0;if(documents>limit/(sizeof(plan_pending)+sizeof(size_t)) || !planCharge(&known,documents*(sizeof(plan_pending)+sizeof(size_t)),limit) || edits>(limit-known)/(sizeof(xide_rename_plan_edit)+2*sizeof(xide_lsp_position)) || !planCharge(&known,edits*(sizeof(xide_rename_plan_edit)+2*sizeof(xide_lsp_position)),limit))goto done;
 scratch=XIDE_RENAME_PLAN_ALLOC(known);why=XIDE_RENAME_PLAN_MEMORY;if(!scratch)goto done;memset(scratch,0,known);pending=scratch;xide_rename_plan_edit* rows=(xide_rename_plan_edit*)(pending+documents);size_t* mapping=(size_t*)(rows+edits);xide_lsp_position* positions=(xide_lsp_position*)(mapping+documents);
 for(size_t i=0;i<documents;++i){
  if(planStopped(&stop))goto done;
  plan_pending* d=pending+i;d->input=inputs+i;d->original=i;++prepared;why=XIDE_RENAME_PLAN_SOURCE;if(!planSource(d,&stop))goto done;
  d->key=xidePathNativeWide(d->input->source.path);d->uri=xideLspPathToUri(d->input->source.path,NULL);why=XIDE_RENAME_PLAN_MEMORY;if(!d->key || !d->uri)goto done;
  why=XIDE_RENAME_PLAN_LIMIT;if(!planCharge(&known,(wcslen(d->key)+1)*sizeof(wchar_t),limit) || !planCharge(&known,strlen(d->uri)+1,limit))goto done;
 }
 qsort(pending,documents,sizeof(*pending),planDocOrder);
 for(size_t i=0;i<documents;++i){why=XIDE_RENAME_PLAN_SOURCE;if(i && !planDocOrder(pending+i-1,pending+i))goto done;mapping[pending[i].original]=i;}
 for(size_t i=0;i<edits;++i){why=XIDE_RENAME_PLAN_RANGE;if(changes[i].document>=documents || changes[i].start.offset<0 || changes[i].end.offset<=changes[i].start.offset)goto done;rows[i]=(xide_rename_plan_edit){mapping[changes[i].document],changes[i].start,changes[i].end,0,0};}
 qsort(rows,edits,sizeof(*rows),planEditOrder);
 size_t bytes=sizeof(*plan),textBytes=0;why=XIDE_RENAME_PLAN_LIMIT;
 if(!planCharge(&bytes,documents*sizeof(plan_document),limit-known) || !planCharge(&bytes,edits*sizeof(xide_rename_plan_edit),limit-known) || !planCharge(&bytes,newName.Size+1,limit-known))goto done;
 plan_count proof={0,limit-known-bytes,&stop};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=PLAN_BYTES;
 size_t next=0;
 for(size_t i=0;i<documents;++i){
  if(planStopped(&stop))goto done;
  plan_pending* d=pending+i;d->first=next;while(next<edits && rows[next].document==i)++next;d->count=next-d->first;why=XIDE_RENAME_PLAN_RANGE;if(!d->count)goto done;
  const char* text=d->input->source.text;size_t previous=0;
  for(size_t j=0;j<d->count;++j){const xide_rename_plan_edit* e=rows+d->first+j;size_t start=(size_t)e->start.offset,end=(size_t)e->end.offset;
   if(start<previous || end>d->beforeSize || end-start!=oldName.Size || memcmp(text+start,oldName.Data,oldName.Size) || (start && xl6LiteralIdentByte((unsigned char)text[start-1])) || (end<d->beforeSize && xl6LiteralIdentByte((unsigned char)text[end])))goto done;
   positions[2*j]=e->start;positions[2*j+1]=e->end;
   if(!e->start.line)positions[2*j].character-=d->input->source.utf16Prefix;
   if(!e->end.line)positions[2*j+1].character-=d->input->source.utf16Prefix;
   previous=end;
  }
  if(!xideLspManyPositionsToOffsets(text,positions,2*d->count,planCancel,&stop))goto done;
  for(size_t j=0;j<d->count;++j)if(positions[2*j].offset!=rows[d->first+j].start.offset || positions[2*j+1].offset!=rows[d->first+j].end.offset)goto done;
  d->afterSize=d->beforeSize-d->count*oldName.Size;why=XIDE_RENAME_PLAN_LIMIT;if(newName.Size>(8u*1024u*1024u-d->afterSize)/d->count)goto done;d->afterSize+=d->count*newName.Size;
  size_t rawBefore=d->beforeSize+3u*(size_t)d->bom,rawAfter=d->afterSize+3u*(size_t)d->bom;
  if(!planCharge(&bytes,strlen(d->input->source.path)+1,limit-known) || !planCharge(&bytes,strlen(d->uri)+1,limit-known) || !planCharge(&bytes,rawBefore+1,limit-known) || !planCharge(&bytes,rawAfter+1,limit-known))goto done;
  if(!planCharge(&textBytes,rawBefore+rawAfter,SIZE_MAX) || !xrtJsonWrite(d->input->document,&config,planCount,&proof))goto done;
 }
 why=XIDE_RENAME_PLAN_LIMIT;if(!planCharge(&known,proof.bytes,limit) || bytes>limit-known)goto done;
 plan=XIDE_RENAME_PLAN_ALLOC(bytes);why=XIDE_RENAME_PLAN_MEMORY;if(!plan)goto done;memset(plan,0,bytes);xrtAtomic32Init(&plan->references,1);plan->info=(xide_rename_plan_info){documents,edits,bytes+proof.bytes,proof.bytes,textBytes};plan->documents=(plan_document*)(plan+1);plan->edits=(xide_rename_plan_edit*)(plan->documents+documents);memcpy(plan->edits,rows,edits*sizeof(*rows));char* out=(char*)(plan->edits+edits);plan->replacement=(xstrview){out,newName.Size};memcpy(out,newName.Data,newName.Size);out[newName.Size]=0;out+=newName.Size+1;
 for(size_t i=0;i<documents;++i){
  if(planStopped(&stop))goto done;
  const plan_pending* d=pending+i;plan_document* document=plan->documents+i;document->beforeSize=d->beforeSize;document->afterSize=d->afterSize;xide_rename_plan_document* v=&document->view;v->hasVersion=d->versioned;v->version=d->version;v->coordinatePrefix=d->input->source.utf16Prefix;v->kind=d->input->source.kind;v->first=d->first;v->count=d->count;size_t length=strlen(d->input->source.path)+1;v->path=out;if(!planCopy(out,d->input->source.path,length,&stop))goto done;out+=length;length=strlen(d->uri)+1;v->uri=out;if(!planCopy(out,d->uri,length,&stop))goto done;out+=length;
  v->before=(xstrview){out,d->beforeSize+3u*(size_t)d->bom};if(d->bom){memcpy(out,"\xef\xbb\xbf",3);out+=3;}if(!planCopy(out,d->input->source.text,d->beforeSize+1,&stop))goto done;out+=d->beforeSize+1;
  v->after=(xstrview){out,d->afterSize+3u*(size_t)d->bom};if(d->bom){memcpy(out,"\xef\xbb\xbf",3);out+=3;}char* body=out;size_t read=0,written=0;
  for(size_t j=0;j<d->count;++j){xide_rename_plan_edit* e=plan->edits+d->first+j;size_t gap=(size_t)e->start.offset-read;if(!planCopy(body+written,d->input->source.text+read,gap,&stop))goto done;written+=gap;e->afterStart=written;if(!planCopy(body+written,newName.Data,newName.Size,&stop))goto done;written+=newName.Size;e->afterEnd=written;read=(size_t)e->end.offset;}
  if(!planCopy(body+written,d->input->source.text+read,d->beforeSize-read+1,&stop))goto done;
  out+=d->afterSize+1;
  v->proof=xrtValueDeepClone(d->input->document);why=XIDE_RENAME_PLAN_MEMORY;if(!v->proof)goto done;
 }
 why=planStopped(&stop) ? XIDE_RENAME_PLAN_CANCELLED : XIDE_RENAME_PLAN_OK;
done:
 for(size_t i=0;i<prepared;++i){free(pending[i].key);free(pending[i].uri);}
 xrtFree(scratch);if(stop.latched)why=XIDE_RENAME_PLAN_CANCELLED;if(why!=XIDE_RENAME_PLAN_OK){xideRenamePlanRelease(plan);plan=NULL;}if(output)*output=why;return plan;
}
int xideRenamePlanInfo(const xide_rename_plan* p,xide_rename_plan_info* out)
{if(out)*out=(xide_rename_plan_info){0};if(!p || !out)return 0;*out=p->info;return 1;}
int xideRenamePlanDocumentAt(const xide_rename_plan* p,size_t at,xide_rename_plan_document* out)
{if(out)*out=(xide_rename_plan_document){0};if(!p || !out || at>=p->info.documents)return 0;*out=p->documents[at].view;return 1;}
int xideRenamePlanEditAt(const xide_rename_plan* p,size_t at,xide_rename_plan_edit* out)
{if(out)*out=(xide_rename_plan_edit){0};if(!p || !out || at>=p->info.edits)return 0;*out=p->edits[at];return 1;}
int xideRenamePlanMapOffset(const xide_rename_plan* p,size_t document,size_t offset,size_t* mapped)
{
 if(mapped)*mapped=0;
 if(!p || !mapped || document>=p->info.documents || offset>p->documents[document].beforeSize)return 0;
 const xide_rename_plan_document* d=&p->documents[document].view;size_t low=0,high=d->count;
 while(low<high){size_t mid=low+(high-low)/2;if((size_t)p->edits[d->first+mid].end.offset<=offset)low=mid+1;else high=mid;}
 if(low<d->count && offset>(size_t)p->edits[d->first+low].start.offset)return 0;
 if(!low)*mapped=offset;else{const xide_rename_plan_edit* e=p->edits+d->first+low-1;*mapped=e->afterEnd>=((size_t)e->end.offset) ? offset+e->afterEnd-(size_t)e->end.offset : offset-((size_t)e->end.offset-e->afterEnd);}return 1;
}
static int planPut(xvalue* value,const char* key,xvalue* child){return child && xrtValueObjectSetNew(value,xrtStrView(key),child);}
static xvalue* planPosition(xide_lsp_position position){xvalue* p=xrtValueObject();if(p && planPut(p,"line",xrtValueInt(position.line)) && planPut(p,"character",xrtValueInt(position.character)))return p;xrtValueRelease(p);return NULL;}
xvalue* xideRenamePlanWorkspaceEdit(const xide_rename_plan* p,int inputProof,int (*cancelled)(void*),void* user)
{
 if(!p || (inputProof!=0 && inputProof!=1))return NULL;
 plan_stop stop={cancelled,user,0};xvalue *result=xrtValueObject(),*documents=xrtValueArray();int okay=result && documents;
 plan_count admission={0,7u*1024u*1024u,&stop};xjsonwriteconfig limits;xrtJsonWriteConfigInit(&limits);limits.MaxOutputBytes=admission.limit;
 for(size_t i=0;okay && i<p->info.documents;++i){
  if(planStopped(&stop)){okay=0;break;}
  const xide_rename_plan_document* d=&p->documents[i].view;xvalue *row=xrtValueObject(),*document=xrtValueObject(),*changes=xrtValueArray();okay=row && document && changes && planPut(document,"uri",xrtValueString(xrtStrView(d->uri))) && planPut(document,"version",d->hasVersion ? xrtValueInt(d->version) : xrtValueNull()) && xrtJsonWrite(document,&limits,planCount,&admission);
  for(size_t j=0;okay && j<d->count;++j){if(planStopped(&stop)){okay=0;break;}const xide_rename_plan_edit* e=p->edits+d->first+j;xvalue *change=xrtValueObject(),*range=xrtValueObject();okay=change && range && planPut(range,"start",planPosition(e->start)) && planPut(range,"end",planPosition(e->end));if(okay){okay=planPut(change,"range",range);range=NULL;}if(okay)okay=planPut(change,"newText",xrtValueString(p->replacement)) && xrtJsonWrite(change,&limits,planCount,&admission);if(okay){okay=xrtValueArrayAppendNew(changes,change);change=NULL;}xrtValueRelease(range);xrtValueRelease(change);}
  if(okay){okay=planPut(row,"textDocument",document);document=NULL;}if(okay){okay=planPut(row,"edits",changes);changes=NULL;}
  if(okay && inputProof){okay=xrtJsonWrite(d->proof,&limits,planCount,&admission);xvalue* proof=okay ? xrtValueObject() : NULL;okay=okay && proof && planPut(proof,"schema",xrtValueInt(1)) && planPut(proof,"document",xrtValueDeepClone(d->proof));if(okay){okay=planPut(row,"xlang",proof);proof=NULL;}xrtValueRelease(proof);}
  if(okay){okay=xrtValueArrayAppendNew(documents,row);row=NULL;}xrtValueRelease(row);xrtValueRelease(document);xrtValueRelease(changes);
 }
 if(okay){okay=planPut(result,"documentChanges",documents);documents=NULL;}xrtValueRelease(documents);
 plan_count bytes={0,7u*1024u*1024u,&stop};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=bytes.limit;if(okay)okay=xrtJsonWrite(result,&config,planCount,&bytes);if(!okay || planStopped(&stop)){xrtValueRelease(result);result=NULL;}return result;
}
#endif
