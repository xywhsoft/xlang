#include "services/diagnostic_notice.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include "platform/receipt_time.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_LIMIT (8u*1024u*1024u)
#define DIAGNOSTIC_LIMIT 4096u
static xstrview sv(const char* s) {return xrtStrView(s);}
static const xvalue* get(const xvalue* v,const char* key)
{return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL;}
static const char* string(const xvalue* value,size_t limit,int nonempty)
{
 xstrview text={0};if(!xrtValueGetString(value,&text) || (nonempty && !text.Size) || text.Size>limit ||
  (text.Size && (!text.Data || memchr(text.Data,0,text.Size) || !xrtUtf8Valid(text,NULL)))) return NULL;
 return text.Data ? text.Data : "";
}
static int integer(const xvalue* value,int64* number,int64 minimum,int64 maximum)
{return value && xrtValueType(value)==XVALUE_INT && xrtValueGetInt(value,number) && *number>=minimum && *number<=maximum;}
static int stopped(int (*cancelled)(void*),void* user) {return cancelled && cancelled(user);}
static int hex(const char* s,size_t length)
{if(!s || strlen(s)!=length) return 0;for(size_t i=0;i<length;++i) if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;return 1;}
static int decimal(const char* s,int signedValue,uint64_t* value)
{
 if(!s || !*s) return 0;
 int negative=signedValue && *s=='-';if(negative) ++s;
 size_t size=strlen(s);if(!size || size>20 || (*s=='0' && (size!=1 || negative))) return 0;
 uint64_t number=0,limit=signedValue ? negative ? (uint64_t)INT64_MAX+1 : (uint64_t)INT64_MAX : UINT64_MAX;
 for(size_t i=0;i<size;++i) {if(s[i]<'0' || s[i]>'9') return 0;unsigned digit=(unsigned)(s[i]-'0');if(number>(limit-digit)/10) return 0;number=number*10+digit;}
 if(value) *value=number;
 return 1;
}
static int samePath(const char* path,const wchar_t* key)
{
 wchar_t* normalized=path && xrtPathIsAbs(path) ? xidePathNativeWide(path) : NULL;
 int same=normalized && !xidePathNativeKeyCompare(normalized,key);free(normalized);return same;
}
static int diskProof(const xvalue* input,const wchar_t* key)
{
 const char* origin=string(get(input,"origin"),16,1);const xvalue* m=get(input,"metadata");int64 raw=0,text=0,kind=0,available=0,attributes=0;
 int schema=xideReceiptMetadataSchema(m);
 if(!origin || strcmp(origin,"disk") || !samePath(string(get(input,"path"),65535,1),key) ||
    !integer(get(input,"rawSize"),&raw,0,TEXT_LIMIT) || !integer(get(input,"textSize"),&text,0,raw) || (raw-text!=0 && raw-text!=3) ||
    !integer(get(input,"kind"),&kind,1,3) || !hex(string(get(input,"rawSha256"),64,1),64) || !hex(string(get(input,"textSha256"),64,1),64) ||
    !integer(get(m,"available"),&available,0,UINT32_MAX) || (available&(XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_IDENTITY))!=(XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_IDENTITY) ||
    !integer(get(m,"attributes"),&attributes,0,UINT32_MAX) || !hex(string(get(m,"device"),16,1),16) || !hex(string(get(m,"identity"),16,1),16) ||
    !schema || !decimal(string(get(m,schema==1 ? "modifiedUs" : "modifiedMs"),21,1),1,NULL) ||
    !decimal(string(get(m,schema==1 ? "changedUs" : "changedMs"),21,1),1,NULL)) return 0;
 return 1;
}
void xideDiagnosticNoticeRelease(xide_diagnostic_notice* n)
{
 if(!n) return;
 if(n->items) for(size_t i=0;i<n->count;++i) xrtFree(n->items[i].title);
 for(size_t i=0;i<n->bindingCount;++i) {free(n->bindings[i].path);free(n->bindings[i].key);}
 xrtFree(n->bindings);
 xrtFree(n->items);free(n->path);free(n->key);xrtValueRelease(n->params);xideDiagnosticDependencyRelease(n->dependency);xrtFree(n);
}
xide_diagnostic_notice* xideDiagnosticNoticeDecode(xide_diagnostic_dependencies* dependencies,const xvalue* params,
 int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
 if(error && capacity) *error=0;
 xide_diagnostic_notice* n=NULL;const char* reason="Invalid diagnostic publication envelope.";
 const xvalue* proof=get(params,"xlang"),*diagnostics=get(params,"diagnostics");int64 schema=0,version=0;bool clear=false;
 const char* uri=string(get(params,"uri"),262144,1);uint64_t generation=0;
 if(!dependencies || !uri || !integer(get(proof,"schema"),&schema,1,1) || !decimal(string(get(proof,"generation"),20,1),0,&generation) ||
    !xrtValueGetBool(get(proof,"cleared"),&clear) || !diagnostics || xrtValueType(diagnostics)!=XVALUE_ARRAY || xrtValueCount(diagnostics)>DIAGNOSTIC_LIMIT ||
    (get(params,"version") && !integer(get(params,"version"),&version,INT_MIN,INT_MAX)) || stopped(cancelled,user)) goto failed;
 n=xrtCalloc(1,sizeof(*n));if(!n) {reason="Cannot allocate diagnostic publication.";goto failed;}
 n->generation=generation;n->cleared=clear;n->version=(int)version;n->versionPresent=get(params,"version")!=NULL;
 if(get(proof,"serial")) {if(!decimal(string(get(proof,"serial"),20,1),0,&n->inputSerial)) goto failed;n->hasSerial=1;}
 n->uri=uri;n->path=xideLspUriToPath(uri,NULL);n->key=n->path ? xidePathNativeWide(n->path) : NULL;
 if(!n->path || !n->key) goto failed;
 if(clear) {
  if(xrtValueCount(diagnostics) || get(proof,"document") || get(proof,"dependencies")) goto failed;
 } else {
  const xvalue* document=get(proof,"document");const char* origin=string(get(document,"origin"),16,1);int64 size=0,wire=0;
  const char* digest=string(get(document,"normalizedSha256"),64,1),*id=string(get(proof,"dependencies"),64,1);
  if(!samePath(string(get(document,"path"),65535,1),n->key) || !origin || !integer(get(document,"normalizedSize"),&size,0,TEXT_LIMIT) || !hex(digest,64)) goto failed;
  const xide_diagnostic_dependency* dependency=xideDiagnosticDependenciesFind(dependencies,id);
  if(!dependency || !xideDiagnosticDependencyContainsKey(dependency,n->key)) {reason="Diagnostic dependency is absent, incomplete or excludes its file.";goto failed;}
  n->overlay=!strcmp(origin,"overlay");
  if(n->overlay) {if(!integer(get(document,"version"),&wire,INT_MIN,INT_MAX) || (n->versionPresent && version!=wire) || get(document,"input")) goto failed;}
  else {
   int64 textSize=0;
   if(strcmp(origin,"disk") || get(document,"version") || n->versionPresent || !diskProof(get(document,"input"),n->key) ||
      !integer(get(get(document,"input"),"textSize"),&textSize,size,TEXT_LIMIT)) goto failed;
  }
  n->version=(int)wire;n->normalizedSize=(size_t)size;memcpy(n->normalizedSha256,digest,65);n->input=get(document,"input");
  n->count=xrtValueCount(diagnostics);n->items=xrtCalloc(n->count+1,sizeof(*n->items));if(!n->items) {reason="Cannot allocate diagnostic items.";goto failed;}
  for(size_t i=0;i<n->count;++i) {
   if(i%16==0 && stopped(cancelled,user)) {reason="Diagnostic decoding cancelled.";goto failed;}
   const xvalue* item=xrtValueArrayGet(diagnostics,i),*range=get(item,"range"),*start=get(range,"start"),*end=get(range,"end");int64 sl=0,sc=0,el=0,ec=0,severity=1;
   const char* message=string(get(item,"message"),65535,0),*source=get(item,"source") ? string(get(item,"source"),65535,0) : "xlang";
   if(!message || !source || !samePath(string(get(item,"file"),65535,1),n->key) ||
      (get(item,"severity") && !integer(get(item,"severity"),&severity,1,4)) ||
      !integer(get(start,"line"),&sl,0,INT_MAX) || !integer(get(start,"character"),&sc,0,INT_MAX) ||
      !integer(get(end,"line"),&el,0,INT_MAX) || !integer(get(end,"character"),&ec,0,INT_MAX) || el<sl || (el==sl && ec<sc)) goto failed;
   xide_diagnostic_item* out=n->items+i;*out=(xide_diagnostic_item){(int)sl,(int)sc,(int)el,(int)ec,(int)severity,-1,-1,message,source,NULL};
   size_t length=strlen(n->path)+strlen(message)+100;out->title=xrtMalloc(length);
   if(!out->title) {reason="Cannot allocate diagnostic title.";goto failed;}
   snprintf(out->title,length,"%s:%lld:%lld · %s",n->path,(long long)sl+1,(long long)sc+1,message);
  }
  n->dependency=xideDiagnosticDependencyRetain(dependency);
  const xvalue* overlays=get(proof,"overlays");
  if(overlays) {
   if(!n->hasSerial || xrtValueType(overlays)!=XVALUE_ARRAY || xrtValueCount(overlays)>64) goto failed;
   n->hasOverlays=1;n->bindings=xrtCalloc(64,sizeof(*n->bindings));if(!n->bindings) goto failed;
   wchar_t* seen[64]={0};size_t used=0;int okay=1;
   for(size_t i=0;okay && i<xrtValueCount(overlays);++i) {
    const xvalue* binding=xrtValueArrayGet(overlays,i);const char* bindingUri=string(get(binding,"uri"),262144,1);int64 bindingVersion=0;
    char* path=bindingUri ? xideLspUriToPath(bindingUri,NULL) : NULL;wchar_t* key=path ? xidePathNativeWide(path) : NULL;
    okay=key && integer(get(binding,"version"),&bindingVersion,INT_MIN,INT_MAX) && !stopped(cancelled,user);
    for(size_t j=0;okay && j<used;++j) if(!xidePathNativeKeyCompare(seen[j],key)) okay=0;
    if(!okay) {free(path);free(key);break;}
    size_t keyBytes=(wcslen(key)+1)*sizeof(wchar_t);seen[used]=malloc(keyBytes);if(!seen[used]) {free(path);free(key);okay=0;break;}
    memcpy(seen[used++],key,keyBytes);
    if(xideDiagnosticDependencyContainsKey(n->dependency,key)) n->bindings[n->bindingCount++]=(xide_diagnostic_binding){bindingUri,path,key,(int)bindingVersion,0,0};
    else {free(path);free(key);}
   }
   for(size_t i=0;i<used;++i) free(seen[i]);
   if(!okay) goto failed;
   if(n->overlay) {
    int own=0;for(size_t i=0;i<n->bindingCount;++i) if(!xidePathNativeKeyCompare(n->key,n->bindings[i].key) && n->version==n->bindings[i].version && !strcmp(n->uri,n->bindings[i].uri)) own=1;
    if(!own) goto failed;
   }
  }
 }
 if(stopped(cancelled,user)) {reason="Diagnostic decoding cancelled.";goto failed;}
 if(!xideDiagnosticDependenciesReference(dependencies,n->path,clear ? NULL : xideDiagnosticDependencyId(n->dependency))) {reason=xideDiagnosticDependenciesError(dependencies);goto failed;}
 n->params=xrtValueRetain((xvalue*)params);return n;
failed:
 if(error && capacity) snprintf(error,capacity,"%s",reason);
 xideDiagnosticNoticeRelease(n);return NULL;
}
/* Hash the existing normalized wire representation with XRT, without creating
   a second body. Bound individual updates and cancellation checks to64KiB. */
static int normalizedDigest(const xls_text* source,char hex[65],size_t* length,int (*cancelled)(void*),void* user)
{
 const char* bytes=xlsTextData(source);size_t size=xlsTextSize(source),at=size>=3 && !memcmp(bytes,"\xef\xbb\xbf",3) ? 3 : 0,total=0;
 xsha256 state;xrtSha256Init(&state);
 while(at<size) {
  if(stopped(cancelled,user)) return 0;
  size_t limit=size-at;if(limit>65536) limit=65536;const char* cr=memchr(bytes+at,'\r',limit);size_t run=cr ? (size_t)(cr-(bytes+at)) : limit;
  if(run && !xrtSha256Update(&state,bytes+at,run)) return 0;
  at+=run;total+=run;
  if(cr) {if(!xrtSha256Update(&state,"\n",1)) return 0;++total;++at;if(at<size && bytes[at]=='\n') ++at;}
 }
 unsigned char digest[32];static const char digits[]="0123456789abcdef";
 if(stopped(cancelled,user) || !xrtSha256Final(&state,digest)) return 0;
 for(size_t i=0;i<32;++i) {hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15];}hex[64]=0;*length=total;return 1;
}
int xideDiagnosticNoticeMap(xide_diagnostic_notice* n,const xls_text* source,int (*cancelled)(void*),void* user)
{
 if(!n || n->cleared || n->mapped || !source || xlsTextSize(source)>TEXT_LIMIT || stopped(cancelled,user)) return 0;
 n->sourceVerified=0;
 char digest[65];size_t size=0;
 if(!normalizedDigest(source,digest,&size,cancelled,user) || size!=n->normalizedSize || strcmp(digest,n->normalizedSha256)) return 0;
 n->sourceVerified=1;
 xide_lsp_position* positions=xrtCalloc(n->count*2+1,sizeof(*positions));if(!positions) return 0;
 int bom=xlsTextSize(source)>=3 && !memcmp(xlsTextData(source),"\xef\xbb\xbf",3),okay=1;
 for(size_t i=0;okay && i<n->count;++i) {
  const xide_diagnostic_item* item=n->items+i;
  if((item->line==0 && bom && item->character==INT_MAX) || (item->endLine==0 && bom && item->endCharacter==INT_MAX)) {okay=0;break;}
  positions[i*2]=(xide_lsp_position){item->line,item->character+(bom && !item->line),0};
  positions[i*2+1]=(xide_lsp_position){item->endLine,item->endCharacter+(bom && !item->endLine),0};
 }
 okay=okay && xideLspManyPositionsToOffsets(xlsTextData(source),positions,n->count*2,cancelled,user) && !stopped(cancelled,user);
 if(okay) {for(size_t i=0;i<n->count;++i) {n->items[i].start=positions[i*2].offset;n->items[i].end=positions[i*2+1].offset;}n->mapped=1;}
 xrtFree(positions);return okay;
}
