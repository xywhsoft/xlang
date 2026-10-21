#include "services/resource_inputs.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define LIMIT (16u*1024u*1024u)
#define DEPTH_LIMIT 512u
typedef struct span { const unsigned char* begin; size_t size; } span;
typedef struct reader { const unsigned char *at,*end; int bad; } reader;
static int fail(xide_model_error* e,const char* s)
{ if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void space(reader* r)
{
 for(;;) {
  while(r->at<r->end && (*r->at==' ' || *r->at=='\t' || *r->at=='\r' || *r->at=='\n')) ++r->at;
  if(r->end-r->at<2 || r->at[0]!='/') return;
  if(r->at[1]=='/') { r->at+=2; while(r->at<r->end && *r->at!='\n') ++r->at; }
  else if(r->at[1]=='*') {
   r->at+=2; while(r->end-r->at>=2 && !(r->at[0]=='*' && r->at[1]=='/')) ++r->at;
   if(r->end-r->at<2) { r->bad=1; r->at=r->end; return; } r->at+=2;
  } else return;
 }
}
static int quoted(reader* r,span* s)
{
 const unsigned char* begin=r->at;
 if(r->at==r->end || *r->at++!='"') return 0;
 while(r->at<r->end) {
  unsigned int c=*r->at++;
  if(c=='"') { *s=(span){begin,(size_t)(r->at-begin)}; return 1; }
  if(c<32) return 0;
  if(c=='\\') { if(r->at==r->end) return 0; ++r->at; }
 }
 return 0;
}
/* Recognize the boundaries of opaque XLON scalars, tags and nested containers.
   This is not a substitute parser: unknown tag payloads/types are never decoded
   into the older GUI runtime, and validity remains the compiler's decision. */
static int value(reader* r,span* s)
{
 space(r); if(r->bad) return 0; const unsigned char* begin=r->at; unsigned char ends[DEPTH_LIMIT]; size_t depth=0;
 while(r->at<r->end) {
  unsigned char c=*r->at;
  if(c=='/' && r->end-r->at>=2 && (r->at[1]=='/' || r->at[1]=='*')) { space(r); if(r->bad) return 0; continue; }
  if(c=='"') { span string; if(!quoted(r,&string)) return 0; continue; }
  if(c=='{' || c=='[' || c=='(') {
   if(depth==DEPTH_LIMIT) return 0;
   ends[depth++]=c=='{' ? '}' : c=='[' ? ']' : ')'; ++r->at; continue;
  }
  if(c=='}' || c==']' || c==')') {
   if(!depth) break;
   if(ends[--depth]!=c) return 0;
   ++r->at; continue;
  }
  if(!depth && c==',') break;
  if(!c) return 0;
  ++r->at;
 }
 if(depth) return 0;
 const unsigned char* end=r->at;
 while(end>begin && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r' || end[-1]=='\n')) --end;
 if(end==begin) return 0;
 *s=(span){begin,(size_t)(end-begin)}; return 1;
}
static xvalue* decode(span s)
{ return xrtJsonParse((xstrview){(const char*)s.begin,s.size}); }
static int string(span s,xvalue** out)
{
 if(!s.size || s.begin[0]!='"') return 0;
 reader r={.at=s.begin,.end=s.begin+s.size}; span token={0};
 if(!quoted(&r,&token)) return -1;
 space(&r); if(r.bad || r.at!=r.end) return -1;
 xvalue* v=decode(token); xstrview text={0};
 if(!v || !xrtValueGetString(v,&text) || memchr(text.Data,0,text.Size)) { xrtValueRelease(v); return -1; }
 *out=v; return 1;
}
/* Only selected fields need DOM string decoding. Object and tag payloads in
   unrelated fields need no conversion, preserving the compiler's value domain. */
static int field(span source,const char* wanted,span* result,int* present)
{
 reader r={.at=source.begin,.end=source.begin+source.size}; space(&r);
 if(r.at==r.end || *r.at++!='{') return 0;
 *present=0;
 for(;;) {
  space(&r); if(r.bad) return 0;
  if(r.at<r.end && *r.at=='}') { ++r.at; space(&r); return !r.bad && r.at==r.end; }
  span key={0},item={0}; if(!quoted(&r,&key)) return 0;
  xvalue* decoded=decode(key); xstrview name={0};
  if(!decoded || !xrtValueGetString(decoded,&name)) { xrtValueRelease(decoded); return 0; }
  int match=name.Size==strlen(wanted) && !memcmp(name.Data,wanted,name.Size); xrtValueRelease(decoded);
  space(&r); if(r.at==r.end || *r.at++!=':' || !value(&r,&item)) return 0;
  if(match) { if(*present) return 0; *present=1; *result=item; }
  space(&r); if(r.at==r.end) return 0;
  if(*r.at=='}') continue;
  if(*r.at++!=',') return 0;
 }
}
void xideResourceInputPathsClear(xide_strings* paths)
{ if(paths) { for(size_t i=0;i<paths->count;++i) free(paths->items[i]); free(paths->items); memset(paths,0,sizeof(*paths)); } }
int xideResourceInputPaths(const unsigned char* bytes,size_t size,const char* configPath,xide_strings* out,xide_model_error* e)
{
 if(!bytes || !size || size>LIMIT || !configPath || !out || memchr(bytes,0,size)) return fail(e,"资源输入为空、超过 16 MiB 或含有 NUL。" );
 if(size>=3 && !memcmp(bytes,"\xef\xbb\xbf",3)) { bytes+=3; size-=3; }
 span root={bytes,size}; xide_strings paths={0}; char* directory=xidePathDirectory(configPath); int okay=directory!=NULL;
 const char* names[]={"icon","manifest"};
 for(int i=0;okay && i<2;++i) {
  span item={0}; int present=0;
  if(!field(root,names[i],&item,&present)) { okay=fail(e,"无法确定资源对象中的素材输入；请检查对象结构或重复的素材键。" ); break; }
  if(!present) continue;
  xvalue* decoded=NULL; int mode=string(item,&decoded);
  if(!mode && item.size && item.begin[0]=='{') {
   span path={0}; int hasPath=0;
   if(!field(item,"path",&path,&hasPath)) { okay=fail(e,"无法确定素材对象中的 path 输入。" ); break; }
   if(hasPath) mode=string(path,&decoded);
  }
  if(mode<0) { okay=fail(e,"素材路径不是有效的无 NUL JSON 字符串。" ); break; }
  if(mode!=1) continue; /* Base64, text, null and other values read no file. */
  xstrview text={0}; xrtValueGetString(decoded,&text);
  char* absolute=text.Size ? xidePathResolve(directory,text.Data) : NULL; xrtValueRelease(decoded);
  if(!absolute) { okay=fail(e,"素材文件路径为空或无法规范化。" ); break; }
  int duplicate=0; for(size_t j=0;j<paths.count;++j) if(!strcmp(paths.items[j],absolute)) duplicate=1;
  if(duplicate) { free(absolute); continue; }
  char** items=realloc(paths.items,(paths.count+1)*sizeof(*items));
  if(!items) { free(absolute); okay=fail(e,"无法分配素材输入列表。" ); break; }
  paths.items=items; paths.items[paths.count++]=absolute; paths.capacity=paths.count;
 }
 free(directory);
 if(!okay) { xideResourceInputPathsClear(&paths); return 0; }
 xideResourceInputPathsClear(out); *out=paths; return 1;
}
