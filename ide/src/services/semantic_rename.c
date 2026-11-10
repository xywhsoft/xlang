#include "services/semantic_rename.h"
#include "analysis/identifier.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "src/lexer/xl6_literal_span.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <limits.h>
#include <string.h>
#ifndef XIDE_RENAME_ALLOC
#define XIDE_RENAME_ALLOC(bytes) xrtCalloc(1,(bytes))
#endif

struct xide_rename_target {
 xatomic32 references;xide_reference_key* key;
 xide_rename_target_info info;
};
typedef struct rename_scan {const unsigned char* text;size_t at,end,sincePoll;int line,column;} rename_scan;
static int stopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static enum xide_rename_target_error advance(rename_scan* s,int line,int column,int (*cancelled)(void*),void* user)
{
 if(line<s->line || (line==s->line && column<s->column))return XIDE_RENAME_TARGET_COORDINATE;
 while(s->line<line || (s->line==line && s->column<column)){
  if(s->sincePoll>=4096){s->sincePoll=0;if(stopped(cancelled,user))return XIDE_RENAME_TARGET_CANCELLED;}
  if(s->at>=s->end)return XIDE_RENAME_TARGET_COORDINATE;
  unsigned char c=s->text[s->at];size_t n=c<128?1:c<0xe0?2:c<0xf0?3:4;
  if(c=='\r' || c=='\n'){
   if(s->line==line)return XIDE_RENAME_TARGET_COORDINATE;
   n=c=='\r' && s->at+1<s->end && s->text[s->at+1]=='\n'?2:1;++s->line;s->column=0;
  }else{
   int units=n==4?2:1;if(s->line==line && units>column-s->column)return XIDE_RENAME_TARGET_COORDINATE;
   s->column+=units;
  }
  if(n>s->end-s->at)return XIDE_RENAME_TARGET_COORDINATE;
  s->at+=n;s->sincePoll+=n;
 }
 return XIDE_RENAME_TARGET_OK;
}
static int utf16Units(const char* text,size_t start,size_t end)
{
 int units=0;for(size_t at=start;at<end;){unsigned char c=(unsigned char)text[at];size_t n=c<128?1:c<0xe0?2:c<0xf0?3:4;at+=n;units+=n==4?2:1;}
 return units;
}
xide_rename_target* xideRenameTargetCreate(xls_projection_store* cache,xls_semantic_check* check,
 const xide_semantic_source* sources,size_t count,const char* path,int line,int character,
 size_t budget,int (*cancelled)(void*),void* user,enum xide_rename_target_error* output)
{
 enum xide_rename_target_error error=XIDE_RENAME_TARGET_ARGUMENT;xide_rename_target* target=NULL;xide_reference_key* key=NULL;
 budget=budget?budget:16u*1024u*1024u;
 if(!check || !sources || !count || !path || line<0 || character<0)goto done;
 if(budget>XIDE_REFERENCE_BYTES || budget<=sizeof(*target)){error=XIDE_RENAME_TARGET_LIMIT;goto done;}
 if(stopped(cancelled,user)){error=XIDE_RENAME_TARGET_CANCELLED;goto done;}
 xide_model_error capture={0};xide_semantic_reference selected={0};
 key=xideSemanticReferenceKeyCreateAt(cache,check,sources,count,path,line,character,budget-sizeof(*target),&selected,&capture);
 if(!key){error=capture.code?XIDE_RENAME_TARGET_CAPTURE:XIDE_RENAME_TARGET_ABSENT;goto done;}
 if(stopped(cancelled,user)){error=XIDE_RENAME_TARGET_CANCELLED;goto done;}
 xide_semantic_source declaration={0};size_t first=0,last=0;
 if(!xideSemanticReferenceKeyTarget(key,&declaration,&first,&last) || !(declaration.kind&1)){error=XIDE_RENAME_TARGET_CAPTURE;goto done;}
 size_t nameBytes=last-first;
 xstrview identifier;int identifierUnits=0;
 if(!xideSemanticReferenceKeyIdentifier(key,&identifier,&identifierUnits)){error=XIDE_RENAME_TARGET_ABSENT;goto done;}
 const xide_semantic_source* query=NULL;
 for(size_t i=0;i<count;++i)if(selected.path && !strcmp(sources[i].path,selected.path)){query=sources+i;break;}
 if(!query || !query->text || selected.start.offset<0 || selected.end.offset<=selected.start.offset ||
    (size_t)selected.end.offset>strlen(query->text)){error=XIDE_RENAME_TARGET_CAPTURE;goto done;}
 rename_scan scan={(const unsigned char*)query->text,(size_t)selected.start.offset,(size_t)selected.end.offset,0,selected.start.line,selected.start.character};
 error=advance(&scan,line,character,cancelled,user);if(error)goto done;
 size_t at=scan.at,begin=at,end=at;
 error=XIDE_RENAME_TARGET_ABSENT;
 if(at>=scan.end || !xl6LiteralIdentByte(scan.text[at]))goto done;
 while(begin>(size_t)selected.start.offset && xl6LiteralIdentByte(scan.text[begin-1])){
  if(at-begin>=nameBytes)goto done;
  --begin;
 }
 while(end<scan.end && xl6LiteralIdentByte(scan.text[end])){
  if(end-begin>=nameBytes)goto done;
  ++end;
 }
 if(end-begin!=nameBytes || memcmp(query->text+begin,declaration.text+first,nameBytes))goto done;
 int before=utf16Units(query->text,begin,at),units=identifierUnits;
 if(before>character || units>INT_MAX-(character-before)){error=XIDE_RENAME_TARGET_COORDINATE;goto done;}
 size_t pathBytes=strlen(query->path)+1,keyBytes=xideSemanticReferenceKeyBytes(key);
 if(keyBytes>budget-sizeof(*target) || pathBytes>budget-sizeof(*target)-keyBytes){error=XIDE_RENAME_TARGET_LIMIT;goto done;}
 target=XIDE_RENAME_ALLOC(sizeof(*target)+pathBytes);if(!target){error=XIDE_RENAME_TARGET_MEMORY;goto done;}
 xrtAtomic32Init(&target->references,1);target->key=key;key=NULL;
 char* owned=(char*)(target+1);memcpy(owned,query->path,pathBytes);
 target->info=(xide_rename_target_info){owned,{line,character-before,(int)begin},{line,character-before+units,(int)end},
  {declaration.text+first,nameBytes},declaration,first,last,sizeof(*target)+pathBytes+keyBytes};
 error=stopped(cancelled,user)?XIDE_RENAME_TARGET_CANCELLED:XIDE_RENAME_TARGET_OK;
done:
 xideSemanticReferenceKeyRelease(key);if(error){xideRenameTargetRelease(target);target=NULL;}
 if(output)*output=error;
 return target;
}
xide_rename_target* xideRenameTargetRetain(xide_rename_target* target)
{if(target)xrtAtomic32FetchAdd(&target->references,1,XMEMORY_RELAXED);return target;}
void xideRenameTargetRelease(xide_rename_target* target)
{if(target && xrtAtomic32FetchSub(&target->references,1,XMEMORY_ACQ_REL)==1){xideSemanticReferenceKeyRelease(target->key);xrtFree(target);}}
int xideRenameTargetInfo(const xide_rename_target* target,xide_rename_target_info* out)
{if(out)*out=(xide_rename_target_info){0};if(!target || !out)return 0;*out=target->info;return 1;}
xide_reference_key* xideRenameTargetRetainKey(const xide_rename_target* target)
{return target?xideSemanticReferenceKeyRetain(target->key):NULL;}
