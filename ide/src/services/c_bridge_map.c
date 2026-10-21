#include "services/c_bridge_map.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define MAP_RECORDS 32768u
static xstrview msv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* mget(const xvalue* v,const char* k) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,msv(k)) : NULL; }
static const char* mstr(const xvalue* v,size_t max) { xstrview s={0};return v && xrtValueGetString(v,&s) && s.Size<=max && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int mequal(const xvalue* v,const char* text) { const char* s=mstr(v,128);return s && !strcmp(s,text); }
static int mint(const xvalue* v,int64* n,int64 low,int64 high) { return v && xrtValueGetInt(v,n) && *n>=low && *n<=high; }
static int mtype(const xvalue* row,int hasTarget)
{
 int64 id=0;const char* abi=mstr(mget(row,"typeAbiId"),16);int nonzero=0;
 if(!mint(mget(row,"typeId"),&id,hasTarget ? 1 : 0,UINT_MAX) || !abi || strlen(abi)!=16) return 0;
 for(int i=0;i<16;++i) { if(!((abi[i]>='0' && abi[i]<='9') || (abi[i]>='a' && abi[i]<='f'))) return 0;nonzero|=abi[i]!='0'; }
 return hasTarget ? nonzero : ((id!=0)==(nonzero!=0));
}
typedef struct type_binding { unsigned symbol,type;const char *name,*abi;const xvalue* target; } type_binding;
static int mcompareType(const void* a,const void* b) { unsigned x=((const type_binding*)a)->symbol,y=((const type_binding*)b)->symbol;return (x>y)-(x<y); }
static int mput(xvalue* v,const char* k,xvalue* item) { return item && xrtValueObjectSetNew(v,msv(k),item); }
static xvalue* mposition(xide_lsp_position p) { xvalue* v=xrtValueObject();if(!v || !mput(v,"line",xrtValueInt(p.line)) || !mput(v,"character",xrtValueInt(p.character))) { xrtValueRelease(v);return NULL; } return v; }
static xvalue* mrange(const xide_semantic_range_query* q) { xvalue* v=xrtValueObject();if(!v || !mput(v,"start",mposition(q->start)) || !mput(v,"end",mposition(q->end))) { xrtValueRelease(v);return NULL; } return v; }
xvalue* xideCBridgeMapBindings(const xvalue* map,const char* declarations,const xide_semantic_source* sources,const xvalue* files,const int* versions,size_t count,xide_model_error* error)
{
 xvalue* result=xrtValueArray();xide_semantic_range_query* ranges=NULL;type_binding* types=NULL;size_t used=0,typeCount=0;int okay=result!=NULL;
 if(!map) return result; /* Older capable ABI providers lack optional navigation. */
 int64 schema=0;bool complete=false;const xvalue* records=mget(map,"records");
 okay=okay && mint(mget(map,"schema"),&schema,1,1) && mequal(mget(map,"scope"),"lowered-public-symbol-source-bindings") && xrtValueGetBool(mget(map,"complete"),&complete) && records && xrtValueType(records)==XVALUE_ARRAY && xrtValueCount(records)<=MAP_RECORDS;
 if(okay && !complete && !xrtValueCount(records) && mequal(mget(map,"reason"),"source-map-unavailable")) return result;
 okay=okay && declarations && sources && versions && count && count<=512 && files && xrtValueType(files)==XVALUE_ARRAY && xrtValueCount(files)==count && mequal(mget(map,"offsetEncoding"),"utf-8-after-bom") && mequal(mget(map,"columnEncoding"),"unicode-scalar-1-based") && mequal(mget(map,"rangeConvention"),"half-open");
 size_t bytes=0,headerSize=declarations ? strlen(declarations) : 0;char* json=okay ? xrtJsonStringify((xvalue*)map,false,&bytes) : NULL;
 okay=okay && json && bytes<=8u*1024u*1024u && headerSize<=1024u*1024u;xrtFree(json);
 size_t total=okay ? xrtValueCount(records) : 0;ranges=calloc(total*2+1,sizeof(*ranges));types=calloc(total+1,sizeof(*types));okay=okay && ranges && types;int64 previous=0;
 for(size_t i=0;okay && i<total;++i) {
  const xvalue* row=xrtValueArrayGet(records,i),*head=mget(row,"headerRange"),*target=mget(row,"target");const char* name=mstr(mget(row,"cName"),1024u*1024u);int64 a=0,z=0,symbol=0,source=0;
  int isType=mequal(mget(row,"kind"),"type"),hasTarget=target && xrtValueType(target)!=XVALUE_NULL;
  okay=name && *name && (mequal(mget(row,"kind"),"function") || mequal(mget(row,"kind"),"global") || isType) && mint(mget(row,"symbolId"),&symbol,1,UINT_MAX) && mint(mget(head,"startOffset"),&a,previous,(int64)headerSize) && mint(mget(head,"endOffset"),&z,a+1,(int64)headerSize) && z-a==(int64)strlen(name) && !memcmp(declarations+a,name,(size_t)(z-a)) && target && (!isType || mtype(row,hasTarget));
  if(!okay) break;
  if(isType) { int64 type=0;mint(mget(row,"typeId"),&type,0,UINT_MAX);types[typeCount++]=(type_binding){(unsigned)symbol,(unsigned)type,name,mstr(mget(row,"typeAbiId"),16),target}; }
  previous=z;
  if(xrtValueType(target)==XVALUE_NULL) { okay=!complete;continue; }
  const xvalue* outer=mget(target,"range"),*inner=mget(target,"selectionRange");int64 start=0,end=0,sa=0,sz=0;
  okay=mint(mget(target,"sourceId"),&source,1,(int64)count) && (sources[source-1].kind&1) && mint(mget(outer,"startOffset"),&start,0,INT_MAX) && mint(mget(outer,"endOffset"),&end,start+1,INT_MAX) && mint(mget(inner,"startOffset"),&sa,start,end-1) && mint(mget(inner,"endOffset"),&sz,sa+1,end);
  if(okay) { ranges[used++]=(xide_semantic_range_query){.sourceId=(int)source,.range=outer};ranges[used++]=(xide_semantic_range_query){.sourceId=(int)source,.range=inner}; }
 }
 if(okay) {
  qsort(types,typeCount,sizeof(*types),mcompareType);
  for(size_t i=1;okay && i<typeCount;++i) if(types[i-1].symbol==types[i].symbol) okay=types[i-1].type==types[i].type && !strcmp(types[i-1].name,types[i].name) && !strcmp(types[i-1].abi,types[i].abi) && xrtValueEqual((xvalue*)types[i-1].target,(xvalue*)types[i].target);
 }
 if(okay) okay=xideSemanticRangesToLsp(sources,count,ranges,used);
 used=0;
 for(size_t i=0;okay && i<total;++i) {
  const xvalue* row=xrtValueArrayGet(records,i),*target=mget(row,"target");if(xrtValueType(target)==XVALUE_NULL) continue;
  int64 source=0;mint(mget(target,"sourceId"),&source,1,(int64)count);const xvalue* file=xrtValueArrayGet(files,(size_t)source-1);const char *sha=mstr(mget(file,"textSha256"),64),*origin=mstr(mget(file,"origin"),16),*path=mstr(mget(file,"path"),32767);int64 size=0;
  okay=sha && strlen(sha)==64 && origin && path && sources[source-1].path && xidePathLexicalEqual(sources[source-1].path,path) && mint(mget(file,"textSize"),&size,0,8u*1024u*1024u) && ((!strcmp(origin,"overlay") && versions[source-1]>0) || (!strcmp(origin,"disk") && !versions[source-1]));
  xvalue* binding=okay ? xrtValueObject() : NULL;
  if(okay) okay=binding && xrtValueObjectSet(binding,msv("headerRange"),mget(row,"headerRange")) && mput(binding,"path",xrtValueString(msv(sources[source-1].path))) && mput(binding,"version",xrtValueInt(versions[source-1])) && mput(binding,"textSize",xrtValueInt(size)) && mput(binding,"textSha256",xrtValueString(msv(sha))) && mput(binding,"range",mrange(&ranges[used])) && mput(binding,"selectionRange",mrange(&ranges[used+1])) && xrtValueArrayAppend(result,binding);
  used+=2;xrtValueRelease(binding);
 }
 free(ranges);free(types);
 if(okay) { size_t size=0;char* encoded=xrtJsonStringify(result,false,&size);okay=encoded && size<=8u*1024u*1024u;xrtFree(encoded); }
 if(!okay) { xrtValueRelease(result);result=NULL;if(error) { error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Optional C/xlang source mapping failed identity, range or capacity validation."); } }
 return result;
}
