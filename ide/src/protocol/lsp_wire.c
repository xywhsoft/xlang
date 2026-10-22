#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#define HEADER_LIMIT 8192u
#define BODY_LIMIT (8u*1024u*1024u)
#define BUFFER_LIMIT (2u*BODY_LIMIT+HEADER_LIMIT)
static int fail(xide_model_error* e,const char* s) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static int utf8(const char* s,size_t n) { return s && n<=INT_MAX && !memchr(s,0,n) && (!n || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,(int)n,NULL,0)); }
void xideLspStreamClear(xide_lsp_stream* s) { if(s) { free(s->bytes); memset(s,0,sizeof(*s)); } }
static int header(xide_lsp_stream* s,xide_model_error* e)
{
 char* end=strstr(s->bytes,"\r\n\r\n"); if(!end) return s->length>HEADER_LIMIT ? fail(e,"LSP header exceeds 8 KiB.") : 1;
 size_t n=(size_t)(end-s->bytes)+4;
 for(size_t i=0;i<n;++i) if((s->bytes[i]=='\r' && (i+1>=n || s->bytes[i+1]!='\n')) || (s->bytes[i]=='\n' && (!i || s->bytes[i-1]!='\r'))) return fail(e,"LSP header has malformed line endings.");
 if(n>HEADER_LIMIT) return fail(e,"LSP header exceeds 8 KiB.");
 char h[HEADER_LIMIT+1]; memcpy(h,s->bytes,n); h[n]=0; size_t body=0; int found=0;
 for(size_t i=0;i<n;++i) if((unsigned char)h[i]>127 || (!h[i]) || ((unsigned char)h[i]<32 && h[i]!='\r' && h[i]!='\n' && h[i]!='\t')) return fail(e,"LSP header is not ASCII.");
 for(char* at=h;*at && at<h+n-2;) {
  char* next=strstr(at,"\r\n"); if(!next) return fail(e,"LSP header has malformed line endings."); *next=0;
  char* colon=strchr(at,':'); if(!colon || colon==at) return fail(e,"LSP header field has no name/value separator."); *colon++=0;
  for(char* p=at;*p;++p) if(!isalnum((unsigned char)*p) && *p!='-') return fail(e,"LSP header field name is invalid.");
  while(*colon==' ' || *colon=='\t') ++colon;
  char* tail=colon+strlen(colon); while(tail>colon && (tail[-1]==' ' || tail[-1]=='\t')) *--tail=0;
  if(!_stricmp(at,"Content-Length")) {
   if(found++ || !*colon) return fail(e,"LSP Content-Length is missing or duplicated.");
   for(char* p=colon;*p;++p) { if(!isdigit((unsigned char)*p) || body>(BODY_LIMIT-(unsigned)(*p-'0'))/10u) return fail(e,"LSP body length is invalid or exceeds 8 MiB."); body=body*10u+(unsigned)(*p-'0'); }
   if(!body) return fail(e,"LSP body cannot be empty.");
  } else if(!_stricmp(at,"Content-Type")) {
   for(char* parameter=strchr(colon,';');parameter;) {
    char* p=parameter+1; while(*p==' ' || *p=='\t') ++p;
    char* next=strchr(p,';'); if(next) *next=0;
    char* equal=strchr(p,'='); if(equal) {
     *equal++=0; char* tail=p+strlen(p); while(tail>p && (tail[-1]==' ' || tail[-1]=='\t')) *--tail=0;
     while(*equal==' ' || *equal=='\t') ++equal;
     tail=equal+strlen(equal); while(tail>equal && (tail[-1]==' ' || tail[-1]=='\t')) *--tail=0;
     if(*equal=='"' && tail>equal+1 && tail[-1]=='"') { ++equal; tail[-1]=0; }
     if(!_stricmp(p,"charset") && _stricmp(equal,"utf-8") && _stricmp(equal,"utf8")) return fail(e,"LSP content encoding must be UTF-8.");
    }
    parameter=next;
   }
  }
  at=next+2;
 }
 if(!found) { return fail(e,"LSP Content-Length is required."); } s->header=n; s->body=body; return 1;
}
int xideLspStreamFeed(xide_lsp_stream* s,const void* data,size_t length,xide_lsp_message_proc receive,void* user,xide_model_error* e)
{
 if(!s || s->failed || (!data && length) || !receive) return fail(e,"Invalid LSP stream state.");
 if(length>BUFFER_LIMIT-s->length) { s->failed=1; return fail(e,"LSP receive buffer exceeds its bound."); }
 size_t need=s->length+length+1; if(need>s->capacity) { size_t cap=need>4096 ? need : 4096; char* p=realloc(s->bytes,cap); if(!p) { s->failed=1; return fail(e,"Cannot allocate LSP stream buffer."); } s->bytes=p; s->capacity=cap; }
 if(length) { memcpy(s->bytes+s->length,data,length); } s->length+=length; s->bytes[s->length]=0;
 while(s->length) {
  if(!s->header && !header(s,e)) goto invalid;
  if(!s->header || s->length-s->header<s->body) return 1;
  const char* body=s->bytes+s->header; if(!utf8(body,s->body)) { fail(e,"LSP body is not valid UTF-8 JSON."); goto invalid; }
  xvalue* value=xrtJsonParse((xstrview){body,s->body}); if(!value || xrtValueType(value)!=XVALUE_OBJECT) { xrtValueRelease(value); fail(e,"LSP body must be a JSON object."); goto invalid; }
  int accepted=receive(value,user); xrtValueRelease(value); if(!accepted) { fail(e,"LSP message receiver rejected the frame."); goto invalid; }
  size_t consumed=s->header+s->body; memmove(s->bytes,s->bytes+consumed,s->length-consumed); s->length-=consumed; s->bytes[s->length]=0; s->header=s->body=0;
 }
 return 1;
invalid: s->failed=1; return 0;
}
int xideLspStreamEnd(xide_lsp_stream* s,xide_model_error* e) { return s && !s->failed && !s->length ? 1 : fail(e,"LSP stream ended in a partial or invalid frame."); }
char* xideLspFrame(const xvalue* value,size_t* length,xide_model_error* e)
{
 size_t n=0; char* json=value && xrtValueType(value)==XVALUE_OBJECT ? xrtJsonStringify(value,false,&n) : NULL; if(!json || !n || n>BODY_LIMIT) { xrtFree(json); fail(e,"Cannot encode a bounded LSP object."); return NULL; }
 char h[100]; int k=snprintf(h,sizeof(h),"Content-Length: %zu\r\n\r\n",n); char* frame=malloc((size_t)k+n); if(!frame) { xrtFree(json); fail(e,"Cannot allocate LSP frame."); return NULL; }
 memcpy(frame,h,(size_t)k); memcpy(frame+k,json,n); xrtFree(json); if(length) *length=(size_t)k+n; return frame;
}
static int absolutePath(const char* p,size_t n)
{
 if(n>=3 && isalpha((unsigned char)p[0]) && p[1]==':' && (p[2]=='/' || p[2]=='\\')) return 1;
 if(n<5 || !((p[0]=='/' && p[1]=='/') || (p[0]=='\\' && p[1]=='\\'))) return 0;
 const char* share=strpbrk(p+2,"/\\"); if(!share || share==p+2 || !share[1] || share[1]=='/' || share[1]=='\\') return 0;
 return 1;
}
static int hex(char c) { return c>='0' && c<='9' ? c-'0' : c>='A' && c<='F' ? c-'A'+10 : c>='a' && c<='f' ? c-'a'+10 : -1; }
char* xideLspPathToUri(const char* path,xide_model_error* e)
{
 if(!path || !utf8(path,strlen(path)) || !absolutePath(path,strlen(path))) { fail(e,"LSP file path must be absolute Windows UTF-8."); return NULL; }
 char* absolute=xidePathResolveLexical(NULL,path); if(!absolute) { fail(e,"Cannot normalize LSP file path."); return NULL; }
 size_t n=strlen(absolute); char* uri=malloc(n*3+10); if(!uri) { free(absolute); return NULL; }
 int unc=absolute[0]=='/' && absolute[1]=='/'; size_t used=(size_t)sprintf(uri,unc ? "file://" : "file:///");
 const char* digits="0123456789ABCDEF"; for(size_t i=unc ? 2u : 0u;i<n;++i) {
  unsigned char c=(unsigned char)absolute[i]; if(c=='\\') c='/';
  if((isalnum(c) && c<128) || strchr("/-._~:",c)) uri[used++]=(char)c;
  else { uri[used++]='%'; uri[used++]=digits[c>>4]; uri[used++]=digits[c&15]; }
 }
 uri[used]=0; free(absolute); return uri;
}
char* xideLspUriToPath(const char* uri,xide_model_error* e)
{
 if(!uri || _strnicmp(uri,"file://",7) || strchr(uri,'?') || strchr(uri,'#') || strchr(uri,'\\')) { fail(e,"Only absolute file URIs are supported."); return NULL; }
 const char* source=uri+7; size_t n=strlen(source); char* path=malloc(n+3); if(!path) return NULL; size_t used=0;
 if(*source=='/') ++source;
 else if(!_strnicmp(source,"localhost/",10)) source+=10;
 else { path[used++]='/'; path[used++]='/'; }
 while(*source) { unsigned char c=(unsigned char)*source++;
  if(c=='%') { if(!source[0] || !source[1] || hex(source[0])<0 || hex(source[1])<0) goto invalid; c=(unsigned char)((hex(source[0])<<4)|hex(source[1])); source+=2; if(!c || c=='/' || c=='\\') goto invalid; }
  if(c<32) { goto invalid; } path[used++]=(char)c;
 }
 path[used]=0; if(!utf8(path,used) || !absolutePath(path,used)) goto invalid;
 char* out=xidePathResolveLexical(NULL,path); free(path); return out;
invalid: free(path); fail(e,"File URI has invalid escaping, encoding or an absolute path."); return NULL;
}
static size_t scalar(const unsigned char* p,unsigned* cp)
{ if(*p<128) { *cp=*p; return 1; } size_t n=*p<0xe0 ? 2u : *p<0xf0 ? 3u : 4u; unsigned c=*p & (n==2 ? 31u : n==3 ? 15u : 7u); for(size_t i=1;i<n;++i) c=(c<<6)|(p[i]&63u); *cp=c; return n; }
int xideLspPositionToOffset(const char* text,int line,int character,int* offset)
{
 if(!text || line<0 || character<0 || !offset || !utf8(text,strlen(text))) { return 0; } size_t n=strlen(text),at=0; int l=0,c=0;
 while(at<n) {
  if(l==line && c==character) { *offset=(int)at; return 1; }
  if(text[at]=='\r' || text[at]=='\n') { if(l==line) return 0; at+=text[at]=='\r' && text[at+1]=='\n' ? 2u : 1u; ++l; c=0; }
  else { unsigned cp; size_t k=scalar((const unsigned char*)text+at,&cp); c+=cp>0xffff ? 2 : 1; if(l==line && c>character) return 0; at+=k; }
 }
 if(l==line && c==character) { *offset=(int)at; return 1; } return 0;
}
int xideLspOffsetToPosition(const char* text,int offset,int* line,int* character)
{
 if(!text || offset<0 || !line || !character || !utf8(text,strlen(text)) || (size_t)offset>strlen(text)) { return 0; } size_t at=0; int l=0,c=0;
 while(at<(size_t)offset) { if(text[at]=='\r' || text[at]=='\n') { at+=text[at]=='\r' && text[at+1]=='\n' ? 2u : 1u; ++l; c=0; } else { unsigned cp; size_t k=scalar((const unsigned char*)text+at,&cp); at+=k; c+=cp>0xffff ? 2 : 1; } if(at>(size_t)offset) return 0; }
 *line=l; *character=c; return 1;
}

typedef struct requested_position { int line,character,offset; size_t index; } requested_position;
static int comparePosition(const void* a,const void* b)
{ const requested_position *x=a,*y=b; if(x->line!=y->line) return x->line<y->line ? -1 : 1; return x->character==y->character ? 0 : x->character<y->character ? -1 : 1; }
static int mapPositions(const char* text,xide_lsp_position* positions,size_t count,size_t limit,int (*cancelled)(void*),void* user)
{
 if(!text || (count && !positions) || count>limit || (cancelled && cancelled(user)) || !utf8(text,strlen(text))) return 0;
 if(!count) return 1;
 requested_position* sorted=calloc(count,sizeof(*sorted)); if(!sorted) return 0;
 for(size_t i=0;i<count;++i) { if(positions[i].line<0 || positions[i].character<0) { free(sorted); return 0; } sorted[i]=(requested_position){.line=positions[i].line,.character=positions[i].character,.index=i}; }
 qsort(sorted,count,sizeof(*sorted),comparePosition); size_t at=0,n=strlen(text); int line=0,character=0,okay=1;
 for(size_t i=0;i<count && okay;++i) {
  if(cancelled && cancelled(user)) { okay=0; break; }
  while(at<n && (line<sorted[i].line || (line==sorted[i].line && character<sorted[i].character))) {
   if(cancelled && !(at&4095u) && cancelled(user)) { okay=0; break; }
   if(text[at]=='\r' || text[at]=='\n') { at+=text[at]=='\r' && text[at+1]=='\n' ? 2u : 1u; ++line; character=0; }
   else { unsigned cp=0; at+=scalar((const unsigned char*)text+at,&cp); character+=cp>0xffff ? 2 : 1; }
  }
  if(line!=sorted[i].line || character!=sorted[i].character) okay=0; else sorted[i].offset=(int)at;
 }
 if(okay && cancelled && cancelled(user)) okay=0;
 if(okay) for(size_t i=0;i<count;++i) positions[sorted[i].index].offset=sorted[i].offset;
 free(sorted); return okay;
}
int xideLspPositionsToOffsets(const char* text,xide_lsp_position* positions,size_t count)
{ return mapPositions(text,positions,count,8192,NULL,NULL); }
int xideLspManyPositionsToOffsets(const char* text,xide_lsp_position* positions,size_t count,int (*cancelled)(void*),void* user)
{ return mapPositions(text,positions,count,131072,cancelled,user); }
