/* Cursor facts are decoded against original source bytes. Never saved indexes. */
#include "services/semantic_completion.h"
#include "platform/path.h"
#include "protocol/lsp_wire.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
typedef struct point { int line,character; } point;
typedef struct span { int source,start,end; point first,last; } span;
typedef struct endpoint { int source,offset,line,column; point* output; } endpoint;
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* k) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(k)) : NULL; }
static int put(xvalue* o,const char* k,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(k),v); }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=INT_MAX && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int equal(const xvalue* v,const char* s) { const char* t=string(v); return t && !strcmp(t,s); }
static int number(const xvalue* v,int64* n,int64 min,int64 max) { return xrtValueGetInt(v,n) && *n>=min && *n<=max; }
static int null(const xvalue* v) { return v && xrtValueType(v)==XVALUE_NULL; }
static int range(const xvalue* value,int source,const xide_semantic_source* sources,size_t count,span* out,endpoint* ends,size_t* used)
{
 if(null(value)) return 1;
 if(!value || source<1 || (size_t)source>count || !(sources[source-1].kind&1)) return 0;
 int64 a,b,sl,sc,el,ec;
 if(!number(get(value,"startOffset"),&a,0,INT_MAX) || !number(get(value,"endOffset"),&b,a,INT_MAX) || !number(get(value,"startLine"),&sl,1,INT_MAX) || !number(get(value,"startColumn"),&sc,1,INT_MAX) || !number(get(value,"endLine"),&el,1,INT_MAX) || !number(get(value,"endColumn"),&ec,1,INT_MAX)) return 0;
 out->source=source; out->start=(int)a; out->end=(int)b;
 ends[(*used)++]=(endpoint){source,(int)a,(int)sl,(int)sc,&out->first};
 ends[(*used)++]=(endpoint){source,(int)b,(int)el,(int)ec,&out->last}; return 1;
}
static int compare(const void* a,const void* b) { const endpoint *x=a,*y=b; return x->source!=y->source ? (x->source>y->source)-(x->source<y->source) : (x->offset>y->offset)-(x->offset<y->offset); }
static int convert(endpoint* ends,size_t used,const xide_semantic_source* sources)
{
 qsort(ends,used,sizeof(*ends),compare); int source=0,line=1,column=1,character=0; size_t at=0,size=0; const char* text=NULL;
 for(size_t i=0;i<used;++i) {
  endpoint* e=&ends[i];
  if(e->source!=source) { source=e->source; text=sources[source-1].text; if(!text) return 0; size=strlen(text); at=0; line=column=1; character=sources[source-1].utf16Prefix; }
  if((size_t)e->offset>size) return 0;
  while(at<(size_t)e->offset) {
   unsigned char c=(unsigned char)text[at]; size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && at+1<size && text[at+1]=='\n') n=2;
   if(n>size-at || at+n>(size_t)e->offset) return 0;
   if(c=='\r' || c=='\n') { ++line; column=1; character=0; } else { ++column; character+=n==4 ? 2 : 1; }
   at+=n;
  }
  if(e->line!=line || e->column!=column) return 0;
  *e->output=(point){line-1,character};
 }
 return 1;
}
static int kind(const char* text)
{ return !strcmp(text,"method") ? 2 : !strcmp(text,"function") ? 3 : !strcmp(text,"field") ? 5 : !strcmp(text,"variable") ? 6 : !strcmp(text,"type") ? 7 : !strcmp(text,"constant") ? 21 : 0; }
static int identifier(const char* text,size_t size)
{ for(size_t i=0;i<size;++i) if((unsigned char)text[i]<128 && text[i]!='_' && !isalnum((unsigned char)text[i])) return 0; return !size || !isdigit((unsigned char)text[0]); }
static xvalue* position(point p) { xvalue* v=xrtValueObject(); if(v && put(v,"line",xrtValueInt(p.line)) && put(v,"character",xrtValueInt(p.character))) return v; xrtValueRelease(v); return NULL; }
static xvalue* lspRange(const span* s) { xvalue* v=xrtValueObject(); if(v && put(v,"start",position(s->first)) && put(v,"end",position(s->last))) return v; xrtValueRelease(v); return NULL; }xvalue* xideSemanticCompletion(const xvalue* query,const xide_semantic_source* sources,size_t count,const char* path,int offset,int version,int available,xide_model_error* error)
{
 int64 schema=0,source=0,reportedOffset=0; bool complete=false,checked=true; const xvalue* items=get(query,"items");
 const char *prefix=string(get(query,"prefix")),*reason=string(get(query,"error")); span replacement={0};
 span* selections=NULL; endpoint* ends=NULL; xvalue *result=NULL,*list=NULL; size_t used=0,bytes=0,itemBytes=0;
 if(!sources || !count || count>512 || !path || !number(get(query,"schema"),&schema,1,1) || !equal(get(query,"scope"),"cursor-query") || !xrtValueGetBool(get(query,"complete"),&complete) || complete!=(available!=0) || !xrtValueGetBool(get(query,"checkedCompilation"),&checked) || checked || !equal(get(query,"offsetEncoding"),"utf-8-after-bom") || !equal(get(query,"columnEncoding"),"unicode-scalar-1-based") || !equal(get(query,"rangeConvention"),"half-open") || !number(get(query,"sourceId"),&source,0,(int64)count) || !number(get(query,"offset"),&reportedOffset,offset,offset) || !items || xrtValueType(items)!=XVALUE_ARRAY || xrtValueCount(items)>4096 || (!available && xrtValueCount(items))) goto invalid;
 if((available && (!prefix || !null(get(query,"error")))) || (!available && (!reason || !*reason || strlen(reason)>1024)) || (!prefix && !null(get(query,"prefix"))) || (prefix && (strlen(prefix)>512 || !identifier(prefix,strlen(prefix))))) goto invalid;
 if(source && (!sources[source-1].path || !xidePathLexicalEqual(path,sources[source-1].path) || !(sources[source-1].kind&1))) goto invalid;
 selections=calloc(xrtValueCount(items)+1,sizeof(*selections)); ends=calloc(xrtValueCount(items)*2+2,sizeof(*ends));
 if(!selections || !ends || !range(get(query,"replacementRange"),(int)source,sources,count,&replacement,ends,&used)) goto invalid;
 if(available || replacement.source) {
  if(!replacement.source || !prefix || replacement.start>offset || replacement.end<offset) goto invalid;
  const char* text=sources[source-1].text; size_t size=text ? strlen(text) : 0,n=strlen(prefix);
  if(!text || (size_t)replacement.end>size || n!=(size_t)(offset-replacement.start) || memcmp(text+replacement.start,prefix,n) || !identifier(text+replacement.start,(size_t)(replacement.end-replacement.start))) goto invalid;
  if(replacement.start==replacement.end) { if(!offset || text[offset-1]!='.') goto invalid; }
  else {
   unsigned char left=replacement.start ? (unsigned char)text[replacement.start-1] : 0,right=(unsigned char)text[replacement.end];
   if((left && (left>=128 || left=='_' || isalnum(left))) || (right && (right>=128 || right=='_' || isalnum(right)))) goto invalid;
  }
 }
 for(size_t i=0;i<xrtValueCount(items);++i) {
  const xvalue* row=xrtValueArrayGet(items,i); const char *name=string(get(row,"name")),*category=string(get(row,"kind")),*type=string(get(row,"canonicalType")); int64 selected=0,symbol=0;
  if(!name || !*name || strlen(name)>512 || !identifier(name,strlen(name)) || !strncmp(name,"__xl6_",6) || strncmp(name,prefix,strlen(prefix)) || !category || !kind(category) || (!type && !null(get(row,"canonicalType"))) || (type && (!*type || strlen(type)>8192)) || !number(get(row,"symbolId"),&symbol,0,UINT_MAX) || !number(get(row,"selectionSourceId"),&selected,0,(int64)count) || !range(get(row,"selectionRange"),(int)selected,sources,count,&selections[i],ends,&used)) goto invalid;
  size_t n=strlen(name)+1+(type ? strlen(type)+1 : 0); if(n>4u*1024u*1024u-bytes) goto invalid; bytes+=n;
 }
 if(!convert(ends,used,sources)) goto invalid;
 result=xrtValueObject(); list=xrtValueArray(); if(!result || !list) goto invalid;
 for(size_t i=0;i<xrtValueCount(items);++i) {
  const xvalue* row=xrtValueArrayGet(items,i); const char *name=string(get(row,"name")),*type=string(get(row,"canonicalType"));
  xvalue *item=xrtValueObject(),*edit=xrtValueObject(),*data=xrtValueObject(),*location=NULL; char order[32]; snprintf(order,sizeof(order),"%04zu",i);
  int okay=item && edit && data && put(item,"label",xrtValueString(sv(name))) && put(item,"kind",xrtValueInt(kind(string(get(row,"kind"))))) && put(item,"insertTextFormat",xrtValueInt(1)) && put(item,"filterText",xrtValueString(sv(name))) && put(item,"sortText",xrtValueString(sv(order))) && (!type || put(item,"detail",xrtValueString(sv(type)))) && put(edit,"range",lspRange(&replacement)) && put(edit,"newText",xrtValueString(sv(name))) && put(data,"symbolId",xrtValueDeepClone(get(row,"symbolId")));
  if(okay && selections[i].source) {
   char* uri=xideLspPathToUri(sources[selections[i].source-1].path,NULL); location=xrtValueObject();
   okay=uri && location && put(location,"uri",xrtValueString(sv(uri))) && put(location,"range",lspRange(&selections[i])); free(uri);
   if(okay) { okay=put(data,"declaration",location); location=NULL; }
  }
  if(okay) { okay=put(item,"textEdit",edit); edit=NULL; } if(okay) { okay=put(item,"data",data); data=NULL; }
  if(okay) {
   /* Declaration URIs may be long and repeat in every item. Measure one
      bounded item before retaining it, never accumulate an oversized list. */
   size_t encodedSize=0; char* encoded=xrtJsonStringify(item,false,&encodedSize);
   okay=encoded && encodedSize+1<=7u*1024u*1024u-16384u-itemBytes;
   xrtFree(encoded); if(okay) itemBytes+=encodedSize+1;
  }
  if(okay) okay=xrtValueArrayAppend(list,item);
  xrtValueRelease(location); xrtValueRelease(edit); xrtValueRelease(data); xrtValueRelease(item); if(!okay) goto invalid;
 }
 if(!put(result,"isIncomplete",xrtValueBool(false)) || !xrtValueObjectSet(result,sv("items"),list) || !put(result,"available",xrtValueBool(available!=0)) || !put(result,"version",xrtValueInt(version)) || !put(result,"scope",xrtValueString(sv("cursor-query"))) || !put(result,"checkedCompilation",xrtValueBool(false)) || !put(result,"guardScope",xrtValueString(sv("background-poll"))) || !put(result,"prefix",prefix ? xrtValueString(sv(prefix)) : xrtValueNull()) || !put(result,"replacementRange",replacement.source ? lspRange(&replacement) : xrtValueNull()) || !put(result,"error",reason ? xrtValueString(sv(reason)) : xrtValueNull())) goto invalid;
 /* JSON duplicates names in label/filter/edit and may expand escapes. Keep
    the entire list bounded, with room for the JSON-RPC envelope/query ID. */
 size_t encodedSize=0; char* encoded=xrtJsonStringify(result,false,&encodedSize);
 int bounded=encoded && encodedSize<=7u*1024u*1024u; xrtFree(encoded); if(!bounded) goto invalid;
 free(selections); free(ends); xrtValueRelease(list); return result;
invalid:
 free(selections); free(ends); xrtValueRelease(list); xrtValueRelease(result);
 if(error) { error->code=XIDE_MODEL_INVALID; snprintf(error->message,sizeof(error->message),"编译器补全的查询位置、来源、类型或范围无法验证。"); } return NULL;
}