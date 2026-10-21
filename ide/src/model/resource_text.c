#include "model/resource_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define LIMIT (16u*1024u*1024u)
#define DEPTH 512u
typedef struct span { const char *at,*end; } span;
typedef struct reader { const char *at,*end; int bad; } reader;
typedef struct field { span prefix,value,tail; int selected; } field;
typedef struct object { span prefix,suffix; field* fields; size_t count,capacity; } object;
typedef struct writer { char* bytes; size_t count,capacity; int bad; } writer;
enum { ROOT, VERSION, ASSET };
static const char* keys[3][19]={
 {"language","codepage","version","icon","manifest",NULL},
 {"comments","companyName","fileDescription","fileVersion","internalName","legalCopyright","originalFilename","productName","productVersion","Comments","CompanyName","FileDescription","FileVersion","InternalName","LegalCopyright","OriginalFilename","ProductName","ProductVersion",NULL},
 {"path","base64","text",NULL}
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static int fail(xide_model_error* e,const char* s)
{ if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void space(reader* r)
{
 for(;;) {
  while(r->at<r->end && (*r->at==' ' || *r->at=='\r' || *r->at=='\n' || *r->at=='\t')) ++r->at;
  if(r->end-r->at<2 || r->at[0]!='/') return;
  if(r->at[1]=='/') { r->at+=2; while(r->at<r->end && *r->at!='\n') ++r->at; }
  else if(r->at[1]=='*') {
   r->at+=2; while(r->end-r->at>=2 && !(r->at[0]=='*' && r->at[1]=='/')) ++r->at;
   if(r->end-r->at<2) { r->bad=1; r->at=r->end; return; } r->at+=2;
  } else return;
 }
}
static int quoted(reader* r,span* out)
{
 const char* begin=r->at; if(r->at==r->end || *r->at++!='"') return 0;
 while(r->at<r->end) {
  unsigned char c=(unsigned char)*r->at++;
  if(c=='"') { *out=(span){begin,r->at}; return 1; }
  if(c<32) return 0;
  if(c=='\\') { if(r->at==r->end) return 0; ++r->at; }
 }
 return 0;
}
/* Find an opaque value's boundary, without converting tags or special floats.
   Only the compiler decides extension tag/type/semantic validity. */
static int value(reader* r,span* out)
{
 const char* begin=r->at; const char* last=begin; char ends[DEPTH]; size_t depth=0;
 while(r->at<r->end) {
  char c=*r->at;
  if(c==' ' || c=='\r' || c=='\n' || c=='\t' || (c=='/' && r->end-r->at>=2 && (r->at[1]=='/' || r->at[1]=='*'))) { space(r); if(r->bad) return 0; continue; }
  if(c=='"') { span s; if(!quoted(r,&s)) return 0; }
  else if(c=='{' || c=='[' || c=='(') { if(depth==DEPTH) return 0; ends[depth++]=c=='{' ? '}' : c=='[' ? ']' : ')'; ++r->at; }
  else if(c=='}' || c==']' || c==')') { if(!depth) break; if(ends[--depth]!=c) return 0; ++r->at; }
  else { if(!depth && c==',') break; ++r->at; }
  last=r->at;
 }
 if(depth || last==begin) return 0;
 *out=(span){begin,last}; return 1;
}
static void clearObject(object* o) { free(o->fields); memset(o,0,sizeof(*o)); }
static int parseObject(span body,int scope,object* out)
{
 reader r={body.at,body.end,0}; space(&r); if(r.bad || r.at==r.end || *r.at++!='{') return 0;
 out->prefix=(span){body.at,r.at}; const char* leading=r.at; unsigned int seen=0;
 for(;;) {
  space(&r); if(r.bad || r.at==r.end) return 0;
  if(*r.at=='}') { out->suffix=(span){leading,body.end}; ++r.at; space(&r); return !r.bad && r.at==r.end; }
  span key={0},item={0}; if(!quoted(&r,&key)) return 0;
  xvalue* name=xrtJsonParse((xstrview){key.at,(size_t)(key.end-key.at)}); xstrview text={0};
  if(!name || !xrtValueGetString(name,&text)) { xrtValueRelease(name); return 0; }
  int selected=-1;
  for(int i=0;keys[scope][i];++i) if(text.Size==strlen(keys[scope][i]) && !memcmp(text.Data,keys[scope][i],text.Size)) selected=i;
  xrtValueRelease(name);
  if(selected>=0) { if(seen&(1u<<selected)) return 0; seen|=1u<<selected; }
  space(&r); if(r.bad || r.at==r.end || *r.at++!=':') return 0; space(&r);
  if(r.bad || !value(&r,&item)) return 0;
  if(out->count==out->capacity) {
   size_t capacity=out->capacity ? out->capacity*2 : 16;
   field* next=realloc(out->fields,capacity*sizeof(*next)); if(!next) return 0;
   out->fields=next; out->capacity=capacity;
  }
  out->fields[out->count++]=(field){{leading,item.at},item,{item.end,r.at},selected};
  if(r.at==r.end) return 0;
  if(*r.at=='}') { leading=r.at; continue; }
  if(*r.at++!=',') return 0;
  leading=r.at;
  /* Match strict object syntax rather than treating a trailing comma as data. */
  reader check=r; space(&check); if(check.at==check.end || *check.at=='}') return 0;
 }
}
static xvalue* project(span body,int scope)
{
 object o={0}; xvalue* root=NULL;
 if(!parseObject(body,scope,&o)) goto done;
 root=xrtValueObject(); if(!root) goto done;
 for(size_t i=0;i<o.count;++i) {
  field* f=&o.fields[i]; if(f->selected<0) continue; const char* key=keys[scope][f->selected]; xvalue* v=NULL;
  if(scope==ROOT && f->value.at[0]=='{' && (!strcmp(key,"version") || !strcmp(key,"icon") || !strcmp(key,"manifest"))) v=project(f->value,!strcmp(key,"version") ? VERSION : ASSET);
  else v=xrtJsonParse((xstrview){f->value.at,(size_t)(f->value.end-f->value.at)});
  if(!v || !xrtValueObjectSetNew(root,sv(key),v)) { xrtValueRelease(root); root=NULL; break; }
 }
done:
 clearObject(&o); return root;
}
void xideResourceTextClear(xide_resource_text* t)
{ if(t) { free(t->original); xrtValueRelease(t->projection); memset(t,0,sizeof(*t)); } }
int xideResourceTextParse(xide_resource_text* t,const char* body,xide_model_error* e)
{
 if(!t || !body || strlen(body)>LIMIT) return fail(e,"资源正文为空、超过 16 MiB 或输入无效。");
 const char* start=body; if(strlen(body)>=3 && !memcmp(body,"\xef\xbb\xbf",3)) start+=3;
 xvalue* root=project((span){start,body+strlen(body)},ROOT);
 if(!root) return fail(e,"资源正文无法按已知字段和 XLON 值边界解析；请在文本编辑器修正。");
 char* original=malloc(strlen(body)+1); if(!original) { xrtValueRelease(root); return fail(e,"无法保留原资源正文。"); }
 strcpy(original,body); xideResourceTextClear(t); t->original=original; t->projection=root; return 1;
}
static xvalue* projectedDraft(const xvalue* original,const xvalue* draft,int scope)
{
 xvalue *a=original && xrtValueType(original)==XVALUE_OBJECT ? xrtValueDeepClone(original) : xrtValueObject(),*z=xrtValueDeepClone(draft),*out=xrtValueObject();
 if(!a || !z || !out) goto failure;
 for(int i=0;keys[scope][i];++i) {
  const char* key=keys[scope][i]; xvalue* now=get(draft,key); xvalue* v=NULL;
  if(now) {
   if(scope==ROOT && xrtValueType(now)==XVALUE_OBJECT && (!strcmp(key,"version") || !strcmp(key,"icon") || !strcmp(key,"manifest"))) v=projectedDraft(original ? get(original,key) : NULL,now,!strcmp(key,"version") ? VERSION : ASSET);
   else v=xrtValueDeepClone(now);
   if(!v || !xrtValueObjectSetNew(out,sv(key),v)) goto failure;
  }
  if(xrtValueObjectHas(a,sv(key)) && !xrtValueObjectRemove(a,sv(key))) goto failure;
  if(xrtValueObjectHas(z,sv(key)) && !xrtValueObjectRemove(z,sv(key))) goto failure;
 }
 if(!xrtValueEqual(a,z)) goto failure;
 xrtValueRelease(a); xrtValueRelease(z); return out;
failure:
 xrtValueRelease(a); xrtValueRelease(z); xrtValueRelease(out); return NULL;
}
xvalue* xideResourceTextDraft(const xvalue* original,const xvalue* draft,xide_model_error* e)
{
 xvalue* out=draft && xrtValueType(draft)==XVALUE_OBJECT ? projectedDraft(original,draft,ROOT) : NULL;
 if(!out) fail(e,"表单只能修改已知资源字段；扩展数据应在文本编辑器中修改。");
 return out;
}
static void writeBytes(writer* w,const char* p,size_t n)
{
 if(w->bad) return;
 if(n>LIMIT-w->count) { w->bad=1; return; }
 size_t need=w->count+n+1;
 if(need>w->capacity) {
  size_t capacity=w->capacity ? w->capacity : 256; while(capacity<need) capacity=capacity>LIMIT/2 ? LIMIT+1 : capacity*2;
  char* next=realloc(w->bytes,capacity); if(!next) { w->bad=1; return; } w->bytes=next; w->capacity=capacity;
 }
 if(n) memcpy(w->bytes+w->count,p,n);
 w->count+=n; w->bytes[w->count]=0;
}
static void writeSpan(writer* w,span s) { writeBytes(w,s.at,(size_t)(s.end-s.at)); }
static void writeJson(writer* w,const xvalue* v)
{ size_t n=0; char* text=xrtJsonStringify(v,false,&n); if(!text) w->bad=1; else writeBytes(w,text,n); xrtFree(text); }
static int patch(writer* w,span body,int scope,const xvalue* before,const xvalue* after)
{
 if(xrtValueEqual(before,after)) { writeSpan(w,body); return !w->bad; }
 object o={0}; if(!parseObject(body,scope,&o)) { clearObject(&o); return 0; }
 unsigned int present=0; size_t projected=0; int written=0;
 /* Reject unprojected modifications; unknown values are owned by the original. */
 for(int i=0;keys[scope][i];++i) if(get(after,keys[scope][i])) ++projected;
 if(projected!=xrtValueCount(after)) { clearObject(&o); return 0; }
 writeSpan(w,o.prefix);
 for(size_t i=0;i<o.count;++i) {
  field* f=&o.fields[i]; const char* key=f->selected<0 ? NULL : keys[scope][f->selected]; xvalue* now=key ? get(after,key) : NULL;
  if(key) { present|=1u<<f->selected; if(!now) continue; }
  if(written++) writeBytes(w,",",1);
  writeSpan(w,f->prefix);
  xvalue* old=key ? get(before,key) : NULL;
  if(!key || xrtValueEqual(old,now)) writeSpan(w,f->value);
  else if(scope==ROOT && f->value.at[0]=='{' && xrtValueType(now)==XVALUE_OBJECT && (!strcmp(key,"version") || !strcmp(key,"icon") || !strcmp(key,"manifest"))) {
   if(!patch(w,f->value,!strcmp(key,"version") ? VERSION : ASSET,old,now)) w->bad=1;
  } else writeJson(w,now);
  writeSpan(w,f->tail);
 }
 for(int i=0;keys[scope][i];++i) {
  xvalue* v=get(after,keys[scope][i]); if(!v || (present&(1u<<i))) continue;
  if(written++) writeBytes(w,",",1);
  writeBytes(w,"\n  \"",4); writeBytes(w,keys[scope][i],strlen(keys[scope][i])); writeBytes(w,"\": ",3); writeJson(w,v);
 }
 writeSpan(w,o.suffix); clearObject(&o); return !w->bad;
}
char* xideResourceTextSerialize(const xide_resource_text* t,const xvalue* draft,xide_model_error* e)
{
 if(!t || !t->original || !t->projection || !draft || xrtValueType(draft)!=XVALUE_OBJECT) { fail(e,"无效资源正文草稿。"); return NULL; }
 writer w={0}; const char* start=t->original; if(strlen(start)>=3 && !memcmp(start,"\xef\xbb\xbf",3)) { writeBytes(&w,start,3); start+=3; }
 int okay=patch(&w,(span){start,t->original+strlen(t->original)},ROOT,t->projection,draft);
 xide_resource_text round={0};
 if(okay) okay=xideResourceTextParse(&round,w.bytes,e) && xrtValueEqual(round.projection,draft);
 xideResourceTextClear(&round);
 char* result=okay ? xrtMalloc(w.count+1) : NULL; if(result) memcpy(result,w.bytes,w.count+1); free(w.bytes);
 if(!result) fail(e,"资源草稿无法保留原 XLON 扩展正文、超出 16 MiB 或分配失败；输入已保留。");
 return result;
}
