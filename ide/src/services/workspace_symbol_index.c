#include "services/workspace_symbol_index.h"
#include "platform/path.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <limits.h>

#define INDEX_BYTES (128u*1024u*1024u)
#define INDEX_PARTS 65536u
#define INDEX_ROWS 1048576u
#define INDEX_RESULTS 4096u
typedef struct symbol_source {const char* path;wchar_t* key;} symbol_source;
typedef struct indexed_symbol {size_t ordinal;char* qualified;size_t leaf;uint64_t mask;unsigned source;int kind,position[8];} indexed_symbol;
typedef struct symbol_part {xatomic32 refs;char* key;xls_context* scope;xide_semantic_symbols* symbols;indexed_symbol* rows;size_t count,bytes;xarray sources;int initialized;} symbol_part;
typedef struct symbol_entry {symbol_part* part;indexed_symbol* row;unsigned scope;} symbol_entry;
struct xide_workspace_symbol_snapshot {xatomic32 refs;xide_workspace_symbol_stats stats;symbol_part** parts;symbol_entry* rows;size_t rowCapacity,resultLimit;};
struct xide_workspace_symbol_index {unsigned refs;int closed;xide_workspace_symbol_limits limits;xide_workspace_symbol_snapshot* current;};
struct xide_workspace_symbol_update {xide_workspace_symbol_index* index;xide_workspace_symbol_snapshot* base;xarray parts;int initialized,failed;int (*stopped)(void*);void* user;};
typedef struct symbol_hit {const symbol_entry* entry;int score;} symbol_hit;
struct xide_workspace_symbol_result {xide_workspace_symbol_snapshot* snapshot;symbol_hit* hits;size_t count,matches;};
typedef struct symbol_scope {const xls_context* context;unsigned next;} symbol_scope;
typedef struct query_term {xstrview text;size_t start,count;} query_term;
typedef struct symbol_query {uint32 scalars[128];query_term terms[16];size_t count,used;uint64_t mask;} symbol_query;

static int error(xide_model_error* e,int code,const char* message){if(e){e->code=code;snprintf(e->message,sizeof(e->message),"%s",message);}return 0;}
static int charge(size_t* bytes,size_t add,size_t limit){if(*bytes>limit || add>limit-*bytes)return 0;*bytes+=add;return 1;}
static int stopped(int (*f)(void*),void* user){return f && f(user);}
static xbytesview keyView(const void* p,size_t n){return (xbytesview){p,n};}
static xstrview text(const char* p){return (xstrview){p,strlen(p)};}
static symbol_part* partRetain(symbol_part* p){if(p)xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_RELAXED);return p;}
static void partRelease(symbol_part* p)
{
 if(!p || xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL)!=1)return;
 for(size_t i=0;p->rows && i<p->count;++i)xrtFree(p->rows[i].qualified);
 symbol_source* sources=(symbol_source*)p->sources.Data;for(size_t i=0;i<p->sources.Count;++i)free(sources[i].key);
 if(p->initialized)xrtArrayUnit(&p->sources);
 xrtFree(p->rows);xrtFree(p->key);xlsContextRelease(p->scope);xideSemanticSymbolsDestroy(p->symbols);xrtFree(p);
}
static int label(const char* p){size_t n=0;if(p)while(n<=262144 && p[n])++n;return n && n<=262144 && xrtUtf8Valid((xstrview){p,n},NULL);}
static uint32 fold(uint32 c){return c>='A' && c<='Z' ? c+('a'-'A') : c;}
static uint64_t mask(const char* p){uint64_t m=0;for(;*p;++p)if((unsigned char)*p<128)m|=UINT64_C(1)<<(fold((unsigned char)*p)&63u);return m;}
static int sourceFor(symbol_part* p,xmap* known,const char* path,size_t limit,unsigned* out)
{
 xbytesview key=keyView(path,strlen(path));const unsigned* found=xrtMapConstGet(known,key);if(found){*out=*found;return 1;}
 if(p->sources.Count>=512 || !xrtPathIsAbs(path))return 0;
 symbol_source source={path,xidePathLexicalWide(path)};if(!source.key)return 0;
 size_t used=p->bytes;
 if(!charge(&used,(wcslen(source.key)+1)*sizeof(wchar_t),limit) ||
    !charge(&used,(p->sources.Count*2+10)*sizeof(symbol_source),limit) || !xrtArrayPush(&p->sources,&source)){free(source.key);return 0;}
 p->bytes+=(wcslen(source.key)+1)*sizeof(wchar_t);*out=(unsigned)p->sources.Count-1;
 return xrtMapSet(known,key,out);
}
static symbol_part* preparePart(const char* key,xls_context* scope,xide_semantic_symbols* symbols,
 const xide_workspace_symbol_limits* limits,int (*stop)(void*),void* user,xide_model_error* e)
{
 const xls_context_spec* spec=xlsContextSpec(scope);size_t count=xideSemanticSymbolsCount(symbols);
 if(!label(key) || !spec || !symbols || !xrtPathIsAbs(spec->entry) || !xrtPathIsAbs(spec->directory) || count>limits->rows){error(e,XIDE_MODEL_INVALID,"Invalid workspace declaration owner,scope or row limit.");return NULL;}
 size_t reserved=sizeof(symbol_part)+strlen(key)+1;
 if(reserved>limits->bytes || count>(limits->bytes-reserved)/sizeof(indexed_symbol)){error(e,XIDE_MODEL_MEMORY,"Workspace owner row storage exceeds its budget.");return NULL;}
 symbol_part* p=xrtCalloc(1,sizeof(*p));if(!p){error(e,XIDE_MODEL_IO,"Cannot allocate workspace declaration owner.");return NULL;}
 xrtAtomic32Init(&p->refs,1);p->scope=xlsContextRetain(scope);p->symbols=xideSemanticSymbolsRetain(symbols);p->count=count;p->bytes=reserved+count*sizeof(indexed_symbol);
 p->key=xrtStrDup(key);p->rows=count ? xrtCalloc(count,sizeof(*p->rows)) : NULL;xmap known={0};int mapped=xrtMapInit(&known,sizeof(unsigned));
 p->initialized=xrtArrayInit(&p->sources,sizeof(symbol_source));int okay=p->key && (!count || p->rows) && mapped && p->initialized;
 for(size_t i=0;okay && i<count;++i){
  if(!(i%64) && stopped(stop,user)){error(e,XIDE_MODEL_CONFLICT,"Workspace declaration preparation cancelled.");okay=0;break;}
  xide_semantic_symbol_view n;indexed_symbol* row=p->rows+i;
  if(!xideSemanticSymbolAt(symbols,i,&n) || !label(n.path) || !label(n.name) || n.kind<1 || n.kind>26 || (n.parent!=SIZE_MAX && n.parent>=i) || !sourceFor(p,&known,n.path,limits->bytes,&row->source)){okay=0;break;}
  size_t parent=n.parent;unsigned depth=0;symbol_source* sources=(symbol_source*)p->sources.Data;
  while(parent!=SIZE_MAX && xidePathNativeKeyCompare(sources[p->rows[parent].source].key,sources[row->source].key)){
   xide_semantic_symbol_view ancestor;if(++depth>256 || !xideSemanticSymbolAt(symbols,parent,&ancestor)){okay=0;break;}parent=ancestor.parent;
  }
  if(!okay)break;
  size_t prefix=parent==SIZE_MAX ? 0 : strlen(p->rows[parent].qualified)+1,names=strlen(n.name);
  size_t sourceBytes=p->sources.Capacity*sizeof(symbol_source);
  if(sourceBytes>limits->bytes || prefix>SIZE_MAX-names-1 || !charge(&p->bytes,prefix+names+1,limits->bytes-sourceBytes)){okay=0;break;}
  row->qualified=xrtMalloc(prefix+names+1);if(!row->qualified){okay=0;break;}
  if(prefix){memcpy(row->qualified,p->rows[parent].qualified,prefix-1);row->qualified[prefix-1]='.';}memcpy(row->qualified+prefix,n.name,names+1);
  row->leaf=prefix;row->ordinal=i;row->kind=n.kind;row->mask=mask(row->qualified);
  int positions[]={n.selectionStart.line,n.selectionStart.character,n.selectionEnd.line,n.selectionEnd.character,n.start.line,n.start.character,n.end.line,n.end.character};memcpy(row->position,positions,sizeof(positions));
 }
 if(mapped)xrtMapUnit(&known);
 okay=okay && charge(&p->bytes,p->sources.Capacity*sizeof(symbol_source),limits->bytes) && !stopped(stop,user);
 if(!okay){partRelease(p);if(!e || !e->code)error(e,XIDE_MODEL_IO,"Workspace declaration preparation failed or exceeded its storage budget.");return NULL;}return p;
}
xide_workspace_symbol_snapshot* xideWorkspaceSymbolSnapshotRetain(xide_workspace_symbol_snapshot* s)
{if(s)xrtAtomic32FetchAdd(&s->refs,1,XMEMORY_RELAXED);return s;}
void xideWorkspaceSymbolSnapshotRelease(xide_workspace_symbol_snapshot* s)
{if(!s || xrtAtomic32FetchSub(&s->refs,1,XMEMORY_ACQ_REL)!=1)return;for(size_t i=0;i<s->stats.parts;++i)partRelease(s->parts[i]);xrtFree(s->parts);xrtFree(s->rows);xrtFree(s);}
static void indexRelease(xide_workspace_symbol_index* index)
{if(!index || --index->refs)return;xideWorkspaceSymbolSnapshotRelease(index->current);xrtFree(index);}
xide_workspace_symbol_index* xideWorkspaceSymbolIndexCreate(const xide_workspace_symbol_limits* requested)
{
 xide_workspace_symbol_limits limits=requested ? *requested : (xide_workspace_symbol_limits){0};
 if(!limits.bytes)limits.bytes=INDEX_BYTES;
 if(!limits.parts)limits.parts=INDEX_PARTS;
 if(!limits.rows)limits.rows=INDEX_ROWS;
 if(!limits.results)limits.results=INDEX_RESULTS;
 if(limits.bytes<sizeof(xide_workspace_symbol_snapshot)+sizeof(xide_workspace_symbol_index) || limits.bytes>512u*1024u*1024u || limits.parts>INDEX_PARTS || limits.rows>INDEX_ROWS || limits.results>INDEX_RESULTS)return NULL;
 xide_workspace_symbol_index* index=xrtCalloc(1,sizeof(*index));if(!index)return NULL;index->current=xrtCalloc(1,sizeof(*index->current));if(!index->current){xrtFree(index);return NULL;}
 index->refs=1;index->limits=limits;xrtAtomic32Init(&index->current->refs,1);index->current->stats.bytes=sizeof(*index->current);index->current->resultLimit=limits.results;return index;
}
void xideWorkspaceSymbolIndexDestroy(xide_workspace_symbol_index* index){if(index){index->closed=1;indexRelease(index);}}
xide_workspace_symbol_snapshot* xideWorkspaceSymbolIndexAcquire(xide_workspace_symbol_index* index)
{return index && !index->closed ? xideWorkspaceSymbolSnapshotRetain(index->current) : NULL;}
void xideWorkspaceSymbolIndexAbort(xide_workspace_symbol_update* u)
{
 if(!u)return;
 symbol_part** parts=(symbol_part**)u->parts.Data;for(size_t i=0;i<u->parts.Count;++i)partRelease(parts[i]);if(u->initialized)xrtArrayUnit(&u->parts);
 xideWorkspaceSymbolSnapshotRelease(u->base);indexRelease(u->index);xrtFree(u);
}
xide_workspace_symbol_update* xideWorkspaceSymbolIndexBegin(xide_workspace_symbol_index* index,int (*stop)(void*),void* user,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!index || index->closed){error(e,XIDE_MODEL_CONFLICT,"Workspace symbol index is unavailable.");return NULL;}
 ++index->refs;if(stopped(stop,user) || index->closed){indexRelease(index);error(e,XIDE_MODEL_CONFLICT,"Workspace symbol index is unavailable or cancelled.");return NULL;}
 xide_workspace_symbol_update* u=xrtCalloc(1,sizeof(*u));if(!u){indexRelease(index);error(e,XIDE_MODEL_IO,"Cannot allocate workspace symbol update.");return NULL;}
 u->index=index;u->base=xideWorkspaceSymbolSnapshotRetain(index->current);u->stopped=stop;u->user=user;u->initialized=xrtArrayInit(&u->parts,sizeof(symbol_part*));
 if(!u->initialized || !xrtArrayReserve(&u->parts,u->base->stats.parts)){xideWorkspaceSymbolIndexAbort(u);error(e,XIDE_MODEL_IO,"Cannot retain workspace symbol owner slots.");return NULL;}
 for(size_t i=0;i<u->base->stats.parts;++i){symbol_part* p=partRetain(u->base->parts[i]);if(!xrtArrayPush(&u->parts,&p)){partRelease(p);xideWorkspaceSymbolIndexAbort(u);error(e,XIDE_MODEL_IO,"Cannot retain workspace symbol owner.");return NULL;}}
 return u;
}
static size_t lowerPart(const xide_workspace_symbol_update* u,const char* key)
{symbol_part** parts=(symbol_part**)u->parts.Data;size_t lo=0,hi=u->parts.Count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(strcmp(parts[mid]->key,key)<0)lo=mid+1;else hi=mid;}return lo;}
static int updateValid(xide_workspace_symbol_update* u,xide_model_error* e)
{if(!u || u->failed || u->index->closed || stopped(u->stopped,u->user)){if(u)u->failed=1;return error(e,XIDE_MODEL_CONFLICT,"Workspace symbol update is unavailable,failed or cancelled.");}return 1;}
int xideWorkspaceSymbolIndexSet(xide_workspace_symbol_update* u,const char* owner,xls_context* scope,xide_semantic_symbols* symbols,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!updateValid(u,e))return 0;
 if(!label(owner)){u->failed=1;return error(e,XIDE_MODEL_INVALID,"Invalid workspace owner key.");}
 size_t at=lowerPart(u,owner);symbol_part** parts=(symbol_part**)u->parts.Data;int replace=at<u->parts.Count && !strcmp(parts[at]->key,owner);
 if(replace && parts[at]->symbols==symbols && xlsContextEqual(parts[at]->scope,scope))return 1;
 if(!replace && u->parts.Count>=u->index->limits.parts){u->failed=1;return error(e,XIDE_MODEL_MEMORY,"Workspace owner count exceeds its limit.");}
 symbol_part* part=preparePart(owner,scope,symbols,&u->index->limits,u->stopped,u->user,e);if(!part){u->failed=1;return 0;}
 if(replace){partRelease(parts[at]);parts[at]=part;return 1;}
 if(!xrtArrayInsert(&u->parts,at,&part,1)){partRelease(part);u->failed=1;return error(e,XIDE_MODEL_IO,"Cannot insert workspace symbol owner.");}return 1;
}
int xideWorkspaceSymbolIndexRemove(xide_workspace_symbol_update* u,const char* owner,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!updateValid(u,e))return 0;
 if(!label(owner)){u->failed=1;return error(e,XIDE_MODEL_INVALID,"Invalid workspace owner key.");}
 size_t at=lowerPart(u,owner);symbol_part** parts=(symbol_part**)u->parts.Data;if(at==u->parts.Count || strcmp(parts[at]->key,owner))return 1;
 symbol_part* removed=parts[at];memmove(parts+at,parts+at+1,(u->parts.Count-at-1)*sizeof(*parts));--u->parts.Count;partRelease(removed);return 1;
}
static int compareEntryIdentity(const symbol_entry* a,const symbol_entry* b)
{
 if(a->scope!=b->scope)return (a->scope>b->scope)-(a->scope<b->scope);
 const symbol_source *as=(symbol_source*)a->part->sources.Data+a->row->source,*bs=(symbol_source*)b->part->sources.Data+b->row->source;
 int order=xidePathNativeKeyCompare(as->key,bs->key);if(order)return order;
 for(size_t i=0;i<8;++i)if(a->row->position[i]!=b->row->position[i])return (a->row->position[i]>b->row->position[i])-(a->row->position[i]<b->row->position[i]);
 if(a->row->kind!=b->row->kind)return (a->row->kind>b->row->kind)-(a->row->kind<b->row->kind);
 return strcmp(a->row->qualified,b->row->qualified);
}
static int compareEntry(const void* av,const void* bv)
{const symbol_entry *a=av,*b=bv;int order=compareEntryIdentity(a,b);return order ? order : strcmp(a->part->key,b->part->key);}
static int uniqueCharge(xmap* seen,const void* identity,size_t size,size_t* used,size_t limit)
{xbytesview key=keyView(&identity,sizeof(identity));if(xrtMapHas(seen,key))return 1;unsigned char one=1;return charge(used,size,limit) && xrtMapSet(seen,key,&one);}
static int sortEntries(symbol_entry* rows,size_t count,int (*stop)(void*),void* user)
{
 /* Bounded qsort chunks reuse the platform sorter;merge the publication
    references cooperatively so million-row cancellation is not held behind
    one whole-index library sort. Scratch is count*sizeof(symbol_entry),
    not retained (24MiB at the default row limit on Windows x64). */
 for(size_t at=0;at<count;at+=1024){if(stopped(stop,user))return 0;size_t n=count-at;if(n>1024)n=1024;qsort(rows+at,n,sizeof(*rows),compareEntry);}
 if(count<=1024)return 1;
 symbol_entry* scratch=xrtCalloc(count,sizeof(*scratch));if(!scratch)return 0;
 symbol_entry *input=rows,*output=scratch;int okay=1;
 for(size_t width=1024;width<count && okay;width*=2){
  for(size_t at=0;at<count && okay;at+=width*2){size_t mid=at+width,end=at+width*2;if(mid>count)mid=count;if(end>count)end=count;size_t left=at,right=mid;
   for(size_t i=at;i<end;++i){if(!(i%256) && stopped(stop,user)){okay=0;break;}output[i]=right==end || (left<mid && compareEntry(input+left,input+right)<=0) ? input[left++] : input[right++];}
  }
  symbol_entry* swap=input;input=output;output=swap;
 }
 if(okay && input!=rows)memcpy(rows,input,count*sizeof(*rows));
 xrtFree(scratch);return okay;
}
static xide_workspace_symbol_snapshot* seal(xide_workspace_symbol_update* u,xide_model_error* e)
{
 size_t count=0;symbol_part** parts=(symbol_part**)u->parts.Data;xide_workspace_symbol_limits limits=u->index->limits;
 for(size_t i=0;i<u->parts.Count;++i){if(parts[i]->count>limits.rows-count){error(e,XIDE_MODEL_MEMORY,"Workspace row count exceeds its limit before deduplication.");return NULL;}count+=parts[i]->count;}
 size_t used=sizeof(xide_workspace_symbol_snapshot)+u->parts.Count*sizeof(symbol_part*);
 if(used>limits.bytes || count>(limits.bytes-used)/sizeof(symbol_entry)){error(e,XIDE_MODEL_MEMORY,"Workspace snapshot row slots exceed its budget.");return NULL;}
 used+=count*sizeof(symbol_entry);xide_workspace_symbol_snapshot* s=xrtCalloc(1,sizeof(*s));if(!s){error(e,XIDE_MODEL_IO,"Cannot allocate workspace snapshot.");return NULL;}xrtAtomic32Init(&s->refs,1);
 s->resultLimit=limits.results;s->rowCapacity=count;s->parts=u->parts.Count ? xrtCalloc(u->parts.Count,sizeof(*s->parts)) : NULL;s->rows=count ? xrtCalloc(count,sizeof(*s->rows)) : NULL;
 xmap scopeHeads={0},seen={0};xarray scopes={0};int scoped=xrtMapInit(&scopeHeads,sizeof(unsigned)),tracked=xrtMapInit(&seen,1),arrayed=xrtArrayInit(&scopes,sizeof(symbol_scope));int okay=(!count || s->rows) && (!u->parts.Count || s->parts) && scoped && tracked && arrayed;
 for(size_t i=0;okay && i<u->parts.Count;++i){
  symbol_part* p=parts[i];if(stopped(u->stopped,u->user)){okay=0;break;}
  okay=uniqueCharge(&seen,p,p->bytes,&used,limits.bytes) && uniqueCharge(&seen,p->scope,xlsContextBytes(p->scope),&used,limits.bytes) && uniqueCharge(&seen,p->symbols,xideSemanticSymbolsBytes(p->symbols),&used,limits.bytes);if(!okay)break;
  uint64_t hash=xlsContextHash(p->scope);xbytesview key=keyView(&hash,sizeof(hash));const unsigned* head=xrtMapConstGet(&scopeHeads,key);unsigned id=head ? *head : 0;
  symbol_scope* data=(symbol_scope*)scopes.Data;while(id && !xlsContextEqual(data[id-1].context,p->scope))id=data[id-1].next;
  if(!id){symbol_scope scope={p->scope,head ? *head : 0};if(!xrtArrayPush(&scopes,&scope)){okay=0;break;}id=(unsigned)scopes.Count;if(!xrtMapSet(&scopeHeads,key,&id)){okay=0;break;}}
  s->parts[s->stats.parts++]=partRetain(p);
  for(size_t j=0;j<p->count;++j)s->rows[s->stats.rows++]=(symbol_entry){p,p->rows+j,id};
 }
 if(okay && count)okay=sortEntries(s->rows,count,u->stopped,u->user);
 size_t kept=0;
 for(size_t i=0;okay && i<count;++i){if(!(i%256) && stopped(u->stopped,u->user)){okay=0;break;}if(kept && !compareEntryIdentity(s->rows+kept-1,s->rows+i))++s->stats.duplicateRows;else s->rows[kept++]=s->rows[i];}
 s->stats.rows=kept;s->stats.scopes=scopes.Count;s->stats.bytes=used;
 if(arrayed)xrtArrayUnit(&scopes);
 if(scoped)xrtMapUnit(&scopeHeads);
 if(tracked)xrtMapUnit(&seen);
 if(!okay || stopped(u->stopped,u->user)){xideWorkspaceSymbolSnapshotRelease(s);error(e,stopped(u->stopped,u->user) ? XIDE_MODEL_CONFLICT : XIDE_MODEL_MEMORY,"Workspace snapshot preparation failed,cancelled or exceeded its budget.");return NULL;}return s;
}
int xideWorkspaceSymbolIndexCommit(xide_workspace_symbol_update* u,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 int okay=updateValid(u,e);xide_workspace_symbol_snapshot* s=NULL;
 if(okay && (u->index->current!=u->base || u->base->stats.generation==UINT64_MAX))okay=error(e,XIDE_MODEL_CONFLICT,"Workspace symbol base revision changed or exhausted.");
 if(okay){s=seal(u,e);okay=s!=NULL;}
 if(okay && stopped(u->stopped,u->user))okay=error(e,XIDE_MODEL_CONFLICT,"Workspace symbol commit cancelled.");
 if(okay && (u->index->closed || u->index->current!=u->base))okay=error(e,XIDE_MODEL_CONFLICT,"Workspace symbol base changed before publication.");
 if(okay){s->stats.generation=u->base->stats.generation+1;xide_workspace_symbol_snapshot* old=u->index->current;u->index->current=s;s=NULL;xideWorkspaceSymbolSnapshotRelease(old);}
 xideWorkspaceSymbolSnapshotRelease(s);xideWorkspaceSymbolIndexAbort(u);return okay;
}
xide_workspace_symbol_stats xideWorkspaceSymbolSnapshotStats(const xide_workspace_symbol_snapshot* s){return s ? s->stats : (xide_workspace_symbol_stats){0};}
int xideWorkspaceSymbolSnapshotResolve(const xide_workspace_symbol_snapshot* s,uint64_t generation,size_t ticket,xide_workspace_symbol_view* out)
{
 if(out)*out=(xide_workspace_symbol_view){0};
 if(!s || !out || generation!=s->stats.generation || ticket>=s->stats.rows)return 0;
 const symbol_entry* entry=s->rows+ticket;
 out->owner=entry->part->key;out->scope=entry->part->scope;out->qualified=entry->row->qualified;out->generation=generation;out->ticket=ticket;
 return xideSemanticSymbolAt(entry->part->symbols,entry->row->ordinal,&out->declaration);
}
static int parseQuery(const char* raw,symbol_query* q)
{
 size_t n=0;if(raw)while(n<=512 && raw[n])++n;else n=513;if(n>512 || !xrtUtf8Valid((xstrview){raw,n},NULL))return 0;
 size_t at=0;int inTerm=0;
 while(at<n){uint32 c=0;size_t read=0;if(xrtUtf8Decode((xstrview){raw+at,n-at},&c,&read)!=XUTF_OK || !read)return 0;
  int space=c==' ' || (c>='\t' && c<='\r');if(!space){if(q->used==128)return 0;if(!inTerm){if(q->count==16)return 0;q->terms[q->count++]=(query_term){.text={raw+at,0},.start=q->used};inTerm=1;}query_term* t=q->terms+q->count-1;t->text.Size+=read;++t->count;q->scalars[q->used++]=fold(c);if(c<128)q->mask|=UINT64_C(1)<<(fold(c)&63u);}else inTerm=0;at+=read;
 }return 1;
}
int xideWorkspaceSymbolQueryValid(const char* raw){symbol_query query={0};return parseQuery(raw,&query);}
static int subsequence(const char* value,const uint32* query,size_t count)
{
 size_t size=strlen(value),at=0,matched=0,last=SIZE_MAX,ordinal=0;int score=0;uint32 previous=0;
 while(at<size && matched<count){uint32 c=0;size_t read=0;if(xrtUtf8Decode((xstrview){value+at,size-at},&c,&read)!=XUTF_OK || !read)return -1;
  if(fold(c)==query[matched]){int boundary=!ordinal || previous=='_' || previous=='.' || previous=='-' || previous=='/' || (previous>='a' && previous<='z' && c>='A' && c<='Z');score+=8+(boundary ? 12 : 0)+(last!=SIZE_MAX && ordinal==last+1 ? 8 : 0);last=ordinal;++matched;}previous=c;++ordinal;at+=read;
 }return matched==count ? score : -1;
}
static int termScore(const indexed_symbol* row,const symbol_query* query,const query_term* term)
{
 const char* leaf=row->qualified+row->leaf;xstrview name=text(leaf),qualified=text(row->qualified);
 if(xrtStrCaseEqual(name,term->text))return 30000;
 if(xrtStrCaseStarts(name,term->text))return 24000;
 if(xrtStrCaseFind(name,term->text,0)!=XRT_NPOS)return 20000;
 if(xrtStrCaseEqual(qualified,term->text))return 18000;
 if(xrtStrCaseStarts(qualified,term->text))return 15000;
 if(xrtStrCaseFind(qualified,term->text,0)!=XRT_NPOS)return 12000;
 int n=subsequence(leaf,query->scalars+term->start,term->count);if(n>=0)return 5000+n;
 n=subsequence(row->qualified,query->scalars+term->start,term->count);return n<0 ? -1 : 1000+n;
}
static int compareHits(const void* av,const void* bv)
{
 const symbol_hit *a=av,*b=bv;if(a->score!=b->score)return (a->score<b->score)-(a->score>b->score);
 int order=strcmp(a->entry->row->qualified+a->entry->row->leaf,b->entry->row->qualified+b->entry->row->leaf);
 if(!order)order=strcmp(a->entry->row->qualified,b->entry->row->qualified);
 return order ? order : compareEntry(a->entry,b->entry);
}
static void heapDown(symbol_hit* heap,size_t count)
{for(size_t at=0;;){size_t child=at*2+1;if(child>=count)return;if(child+1<count && compareHits(heap+child+1,heap+child)>0)++child;if(compareHits(heap+at,heap+child)>=0)return;symbol_hit t=heap[at];heap[at]=heap[child];heap[child]=t;at=child;}}
xide_workspace_symbol_result* xideWorkspaceSymbolSnapshotQuery(xide_workspace_symbol_snapshot* s,const char* raw,size_t limit,int (*stop)(void*),void* user,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 symbol_query query={0};if(!s || !limit || limit>s->resultLimit || !parseQuery(raw,&query)){error(e,XIDE_MODEL_INVALID,"Invalid workspace query,term count or result limit.");return NULL;}
 xideWorkspaceSymbolSnapshotRetain(s);
 if(stopped(stop,user)){xideWorkspaceSymbolSnapshotRelease(s);error(e,XIDE_MODEL_CONFLICT,"Workspace symbol query cancelled.");return NULL;}
 xide_workspace_symbol_result* result=xrtCalloc(1,sizeof(*result));if(!result){xideWorkspaceSymbolSnapshotRelease(s);error(e,XIDE_MODEL_IO,"Cannot allocate workspace query result.");return NULL;}
 result->snapshot=s;result->hits=xrtCalloc(limit,sizeof(*result->hits));if(!result->hits){xideWorkspaceSymbolResultDestroy(result);error(e,XIDE_MODEL_IO,"Cannot allocate workspace query slots.");return NULL;}
 for(size_t i=0;i<s->stats.rows;++i){
  if(!(i%128) && stopped(stop,user)){xideWorkspaceSymbolResultDestroy(result);error(e,XIDE_MODEL_CONFLICT,"Workspace symbol query cancelled.");return NULL;}
  const symbol_entry* entry=s->rows+i;if((entry->row->mask&query.mask)!=query.mask)continue;int score=0;
  for(size_t j=0;j<query.count;++j){int term=termScore(entry->row,&query,query.terms+j);if(term<0){score=-1;break;}score+=term;}if(score<0)continue;
  ++result->matches;symbol_hit hit={entry,score};if(result->count<limit){size_t at=result->count++;result->hits[at]=hit;while(at){size_t parent=(at-1)/2;if(compareHits(result->hits+at,result->hits+parent)<=0)break;symbol_hit t=result->hits[at];result->hits[at]=result->hits[parent];result->hits[parent]=t;at=parent;}}
  else if(compareHits(&hit,result->hits)<0){result->hits[0]=hit;heapDown(result->hits,result->count);}
 }
 if(stopped(stop,user)){xideWorkspaceSymbolResultDestroy(result);error(e,XIDE_MODEL_CONFLICT,"Workspace symbol query cancelled.");return NULL;}
 qsort(result->hits,result->count,sizeof(*result->hits),compareHits);
 if(stopped(stop,user)){xideWorkspaceSymbolResultDestroy(result);error(e,XIDE_MODEL_CONFLICT,"Workspace symbol query cancelled.");return NULL;}return result;
}
size_t xideWorkspaceSymbolResultCount(const xide_workspace_symbol_result* r){return r ? r->count : 0;}
size_t xideWorkspaceSymbolResultMatches(const xide_workspace_symbol_result* r){return r ? r->matches : 0;}
int xideWorkspaceSymbolResultAt(const xide_workspace_symbol_result* r,size_t at,xide_workspace_symbol_view* out)
{if(out)*out=(xide_workspace_symbol_view){0};if(!r || !out || at>=r->count)return 0;const symbol_hit* hit=r->hits+at;int okay=xideWorkspaceSymbolSnapshotResolve(r->snapshot,r->snapshot->stats.generation,(size_t)(hit->entry-r->snapshot->rows),out);if(okay)out->score=hit->score;return okay;}
void xideWorkspaceSymbolResultDestroy(xide_workspace_symbol_result* r){if(r){xideWorkspaceSymbolSnapshotRelease(r->snapshot);xrtFree(r->hits);xrtFree(r);}}
