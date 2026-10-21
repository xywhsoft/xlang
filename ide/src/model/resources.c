#include "model/resources.h"
#include "platform/path.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#define LIMIT (16u*1024u*1024u)
static const char* camel[]={"comments","companyName","fileDescription","fileVersion","internalName","legalCopyright","originalFilename","productName","productVersion"};
static const char* official[]={"Comments","CompanyName","FileDescription","FileVersion","InternalName","LegalCopyright","OriginalFilename","ProductName","ProductVersion"};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static int fail(xide_model_error* e,const char* message) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) ? s.Data : ""; }
static int validString(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) && s.Size<=LIMIT && !memchr(s.Data,0,s.Size); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int erase(xvalue* v,const char* k) { return !xrtValueObjectHas(v,sv(k)) || xrtValueObjectRemove(v,sv(k)); }
static int decimal(const char* s,unsigned int* result)
{
 uint64_t n=0; if(!s || !*s) return 0;
 for(size_t i=0;s[i];++i) { if(s[i]<'0' || s[i]>'9') return 0; n=n*10+(unsigned int)(s[i]-'0'); if(n>UINT_MAX) return 0; }
 *result=(unsigned int)n; return 1;
}
static int number(const xvalue* v,unsigned int* n)
{
 int64 i=0; if(v && xrtValueGetInt(v,&i)) { if(i<0 || (uint64_t)i>UINT_MAX) return 0; *n=(unsigned int)i; return 1; }
 xrtClearError(); return v && validString(v) && decimal(string(v),n);
}
static int versionQuad(const char* s)
{
 if(!*s) return 1;
 for(int part=0;part<4;++part) {
  if(*s<'0' || *s>'9') return 0;
  unsigned int n=0;
  while(*s>='0' && *s<='9') { n=n*10+(unsigned int)(*s++-'0'); if(n>65535) return 0; }
  if(!*s) return 1;
  if(*s++!='.') return 0;
 }
 /* The current compiler also accepts a trailing dot after the fourth part. */
 return !*s;
}
const char* xideResourceVersion(const xvalue* root,int i)
{
 if(i<0 || i>=9) return "";
 xvalue* v=get(root,"version"); xvalue* c=get(v,camel[i]); return c && xrtValueType(c)==XVALUE_STRING ? string(c) : string(get(v,official[i]));
}
unsigned int xideResourceNumber(const xvalue* root,const char* key)
{ unsigned int n=!strcmp(key,"language") ? 1033u : 1200u; unsigned int parsed=0; return number(get(root,key),&parsed) ? parsed : n; }
int xideResourceSource(const xvalue* root,int manifest,const char** payload)
{
 xvalue* v=get(root,manifest ? "manifest" : "icon"); if(payload) *payload="";
 if(!v || xrtValueType(v)==XVALUE_NULL) return XIDE_RESOURCE_NONE;
 if(xrtValueType(v)==XVALUE_STRING) { if(payload) *payload=string(v); return XIDE_RESOURCE_PATH; }
 const char* keys[]={"path","base64","text"};
 for(int i=0;i<(manifest ? 3 : 2);++i) { xvalue* f=get(v,keys[i]); if(f && xrtValueType(f)==XVALUE_STRING) { if(payload) *payload=string(f); return i+1; } }
 return -1;
}
int xideResourceValidate(const xvalue* root,xide_model_error* e)
{
 if(!root || xrtValueType(root)!=XVALUE_OBJECT) return fail(e,"资源配置必须是对象；原正文已保留。");
 const char* nums[]={"language","codepage"}; for(int i=0;i<2;++i) { xvalue* v=get(root,nums[i]); unsigned int n=0; if(v && !number(v,&n)) return fail(e,"language/codepage 必须是 0–4294967295 的十进制整数或字符串。"); }
 xvalue* version=get(root,"version");
 if(version && xrtValueType(version)!=XVALUE_NULL) {
  if(xrtValueType(version)!=XVALUE_OBJECT) return fail(e,"version 必须是对象或 null；请在文本编辑器修正。");
  for(int i=0;i<9;++i) {
   xvalue *a=get(version,camel[i]),*b=get(version,official[i]);
   if((a && !validString(a)) || (b && !validString(b))) return fail(e,"版本字段必须是无 NUL 的字符串；请在文本编辑器修正。");
  }
  if(!versionQuad(xideResourceVersion(root,3)) || !versionQuad(xideResourceVersion(root,8))) return fail(e,"版本号需为 1–4 个 0–65535 的十进制段，例如 1.2.3.4。");
 }
 for(int manifest=0;manifest<2;++manifest) {
  xvalue* v=get(root,manifest ? "manifest" : "icon"); if(!v || xrtValueType(v)==XVALUE_NULL) continue;
  if(xrtValueType(v)==XVALUE_STRING) { if(!validString(v)) return fail(e,"素材路径包含无效字符串。"); }
  else if(xrtValueType(v)==XVALUE_OBJECT) {
   const char* keys[]={"path","base64","text"}; for(int k=0;k<3;++k) { xvalue* s=get(v,keys[k]); if(s && !validString(s)) return fail(e,"素材 path/base64/text 必须是无 NUL 的字符串。"); }
  } else return fail(e,"icon/manifest 必须是路径字符串、素材对象或 null。");
  const char* payload=NULL; if(xideResourceSource(root,manifest,&payload)<0) return fail(e,"素材对象缺少 path/base64；manifest 还支持 text。");
 }
 return 1;
}
void xideResourceClear(xide_resource_config* c) { if(c) { xrtValueRelease(c->root); xideResourceTextClear(&c->text); memset(c,0,sizeof(*c)); } }
int xideResourceParse(xide_resource_config* c,const char* text,xide_model_error* e)
{
 if(!c || !text || strlen(text)>LIMIT) return fail(e,"资源配置超过 16 MiB 或输入无效。");
 /* JSON keeps its compatible DOM. XLON extensions remain opaque source spans
    owned by resource_text, even when a newer runtime can decode their types. */
 xvalue* root=xrtJsonParse(sv(text)); int xson=root==NULL;
 if(!root) xrtClearError();
 xide_resource_text source={0};
 xrtClearError(); if(!xideResourceTextParse(&source,text,e)) { xrtValueRelease(root); return 0; }
 if(!root) root=xrtValueDeepClone(source.projection);
 if(!root || !xideResourceValidate(root,e)) { xrtValueRelease(root); xideResourceTextClear(&source); return 0; }
 xideResourceClear(c); c->root=root; c->xson=xson; c->text=source; return 1;
}
int xideResourceSetVersion(xvalue* root,int enabled,const char* const fields[9],xide_model_error* e)
{
 if(!root || xrtValueType(root)!=XVALUE_OBJECT || (enabled && !fields)) return fail(e,"无效版本草稿。");
 if(!enabled) { xvalue* existing=get(root,"version"); if(!existing || xrtValueType(existing)==XVALUE_NULL) return 1; return erase(root,"version") ? 1 : fail(e,"无法移除版本段。"); }
 xvalue* old=get(root,"version"); xvalue* v=old && xrtValueType(old)==XVALUE_OBJECT ? xrtValueDeepClone(old) : xrtValueObject();
 if(!v) return fail(e,"无法准备版本草稿。");
 for(int i=0;i<9;++i) {
  if(!fields[i]) { xrtValueRelease(v); return fail(e,"缺少版本字段。"); }
  if(strcmp(fields[i],xideResourceVersion(root,i))) {
   const char* key=get(v,camel[i]) ? camel[i] : get(v,official[i]) ? official[i] : camel[i];
   if(!put(v,key,fields[i])) { xrtValueRelease(v); return fail(e,"无法更新版本字段。"); }
  }
 }
 return xrtValueObjectSetNew(root,sv("version"),v) ? 1 : fail(e,"无法更新版本段。");
}
int xideResourceSetNumber(xvalue* root,const char* k,const char* decimalText,xide_model_error* e)
{
 if(!root || xrtValueType(root)!=XVALUE_OBJECT || !k) return fail(e,"无效数值草稿。");
 if(strcmp(k,"language") && strcmp(k,"codepage")) return fail(e,"未知数值字段。");
 unsigned int n=0; if(!decimal(decimalText,&n)) return fail(e,"language/codepage 必须是 0–4294967295 的十进制整数。");
 if(n==xideResourceNumber(root,k)) return 1;
 xvalue* old=get(root,k); int okay=old && xrtValueType(old)==XVALUE_STRING ? put(root,k,decimalText) : xrtValueObjectSetNew(root,sv(k),xrtValueInt((int64)n));
 return okay ? 1 : fail(e,"无法更新资源数值。");
}
int xideResourceSetSource(xvalue* root,int manifest,int mode,const char* payload,xide_model_error* e)
{
 if(!root || xrtValueType(root)!=XVALUE_OBJECT) return fail(e,"无效素材草稿。");
 if(!payload || mode<0 || mode>(manifest ? 3 : 2)) return fail(e,"素材类型无效。");
 const char* previous=NULL; int oldMode=xideResourceSource(root,manifest,&previous);
 if(mode==oldMode && !strcmp(payload,previous)) return 1;
 const char* key=manifest ? "manifest" : "icon";
 if(!mode) return erase(root,key) ? 1 : fail(e,"无法移除素材段。");
 xvalue* old=get(root,key);
 if(mode==XIDE_RESOURCE_PATH && old && xrtValueType(old)==XVALUE_STRING) return put(root,key,payload) ? 1 : fail(e,"无法更新素材路径。");
 xvalue* v=old && xrtValueType(old)==XVALUE_OBJECT ? xrtValueDeepClone(old) : xrtValueObject();
 const char* keys[]={"path","base64","text"}; int okay=v!=NULL;
 for(int i=0;okay && i<3;++i) okay=erase(v,keys[i]);
 if(okay) okay=put(v,keys[mode-1],payload);
 if(!okay) { xrtValueRelease(v); return fail(e,"无法准备素材草稿。"); }
 return xrtValueObjectSetNew(root,sv(key),v) ? 1 : fail(e,"无法更新素材段。");
}
char* xideResourceSerialize(const xide_resource_config* c,const xvalue* draft,xide_model_error* e)
{
 if(!c || !xideResourceValidate(draft,e)) return NULL;
 if(c->text.original) {
  xvalue* projection=xideResourceTextDraft(c->root,draft,e); if(!projection) return NULL;
  char* body=xideResourceTextSerialize(&c->text,projection,e); xrtValueRelease(projection); return body;
 }
 size_t n=0; char* body=c->xson ? xrtXlonStringify(draft,true,&n) : xrtJsonStringify(draft,true,&n);
 if(!body || n>LIMIT) { xrtFree(body); fail(e,"资源序列化失败或超过 16 MiB；草稿已保留。"); return NULL; }
 xvalue* round=c->xson ? xrtXlonParse((xstrview){body,n}) : xrtJsonParse((xstrview){body,n});
 int equal=round && xrtValueEqual(round,draft); xrtValueRelease(round);
 if(!equal) { xrtFree(body); fail(e,"资源值无法无损往返；请保留原正文并用文本编辑器处理。"); return NULL; }
 return body;
}
static unsigned int u16(const unsigned char* b) { return b[0]|((unsigned int)b[1]<<8); }
static uint32_t u32(const unsigned char* b) { return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); }
int xideResourceCheckAssets(const xvalue* root,const char* path,xide_model_error* e)
{
 if(!xideResourceValidate(root,e)) return 0;
 xvalue* version=get(root,"version"); int count=version && xrtValueType(version)==XVALUE_OBJECT;
 for(int manifest=0;manifest<2;++manifest) {
  const char* s=NULL; int mode=xideResourceSource(root,manifest,&s); if(!mode) continue; ++count;
  unsigned char* owned=NULL; const unsigned char* bytes=NULL; size_t n=0;
  if(mode==XIDE_RESOURCE_PATH) {
   char* base=xidePathDirectory(path); char* absolute=base ? xidePathResolve(base,s) : NULL; free(base);
   if(absolute && !xidePathIsDirectory(absolute)) { owned=xrtFileReadAllLimit(absolute,LIMIT,&n); bytes=owned; } free(absolute);
  } else if(mode==XIDE_RESOURCE_BASE64) { if(strlen(s)<=LIMIT) { owned=xrtBase64DecodeNew(s,strlen(s),&n,NULL); bytes=owned; } }
  else { bytes=(const unsigned char*)s; n=strlen(s); }
  int okay=bytes && n>0 && n<=LIMIT;
  if(okay && !manifest) {
   okay=n>=6 && u16(bytes)==0 && u16(bytes+2)==1 && u16(bytes+4)>0 && u16(bytes+4)<=(n-6)/16;
   for(unsigned int i=0;okay && i<u16(bytes+4);++i) { const unsigned char* item=bytes+6+i*16; uint32_t length=u32(item+8),offset=u32(item+12); okay=length>0 && offset<=n && length<=n-offset; }
  }
  xrtFree(owned);
  if(!okay) return fail(e,manifest ? "应用清单不可读取、Base64 无效、为空或超过 16 MiB。路径相对于资源配置。" : "图标素材不可读取、Base64 无效、超过 16 MiB，或 ICO 目录/图像范围无效。");
 }
 return count ? 1 : fail(e,"配置至少需要版本信息、图标或应用清单。");
}
