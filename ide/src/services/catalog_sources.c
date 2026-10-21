#include "services/catalog_sources.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
int xideCatalogDigest(const void* bytes,size_t size,char digest[65])
{
    unsigned char raw[32]; static const char hex[]="0123456789abcdef";
    if(!digest || (!bytes && size) || size>ULONG_MAX || BCryptHash(BCRYPT_SHA256_ALG_HANDLE,NULL,0,(PUCHAR)bytes,(ULONG)size,raw,sizeof(raw))<0) return 0;
    for(int i=0;i<32;++i) { digest[2*i]=hex[raw[i]>>4]; digest[2*i+1]=hex[raw[i]&15]; } digest[64]=0; return 1;
}
static int fail(xide_model_error* e,const char* message)
{ if(e) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static xvalue* get(xvalue* value,const char* key)
{ return value && xrtValueIs(value,XVALUE_OBJECT) ? xrtValueObjectGet(value,(xstrview){key,strlen(key)}) : NULL; }
static int equal(xvalue* value,const char* text)
{ xstrview view={0}; return value && xrtValueGetString(value,&view) && view.Size==strlen(text) && !memcmp(view.Data,text,view.Size); }
static int hash(xvalue* value,char digest[65])
{
    xstrview view={0}; if(!value || !xrtValueGetString(value,&view) || view.Size!=64) return 0;
    for(size_t i=0;i<64;++i) if(!((view.Data[i]>='0' && view.Data[i]<='9') || (view.Data[i]>='a' && view.Data[i]<='f'))) return 0;
    memcpy(digest,view.Data,64); digest[64]=0; return 1;
}
int xideCatalogSourceRead(const xide_command_catalog* catalog,const char* source,xide_text_file* file,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!file) return fail(error,"来源查看参数无效。");
    xideTextFileClear(file);
    if(!catalog || !catalog->path || !source) return fail(error,"请先选择命令或帮助文档。");
    xvalue* sources=get(catalog->source,"sources"); char expected[65]={0}; int found=0;
    for(size_t i=0;sources && i<xrtValueCount(sources);++i) {
        xvalue* row=xrtValueArrayGet(sources,i);
        if(equal(get(row,"path"),source)) { if(found || !hash(get(row,"sha256"),expected)) return fail(error,"目录来源摘要无效或重复。"); found=1; }
    }
    if(!found) return fail(error,"命令目录没有记录该来源的内容摘要。");
    char* base=xidePathDirectory(catalog->path); char* folder=base ? xidePathResolve(base,"catalog-sources") : NULL; free(base);
    char* index=folder ? xidePathResolve(folder,"index.json") : NULL; size_t size=0;
    unsigned char* bytes=index ? xideIoReadAllLimit(index,1024u*1024u,&size) : NULL; free(index);
    xvalue* root=bytes ? xrtJsonParse((xstrview){(char*)bytes,size}) : NULL; xrtFree(bytes);
    if(!root || !equal(get(root,"format"),"xlang-catalog-sources-1") || !equal(get(root,"catalogSha256"),catalog->sha256)) { xrtValueRelease(root); free(folder); return fail(error,"来源资料未安装，或与当前命令目录不匹配。"); }
    xvalue* entries=get(root,"entries"); xvalue* matched=NULL; int count=0;
    if(entries && xrtValueIs(entries,XVALUE_ARRAY)) for(size_t i=0;i<xrtValueCount(entries);++i) {
        xvalue* row=xrtValueArrayGet(entries,i); if(equal(get(row,"path"),source)) { matched=row; ++count; }
    }
    char indexed[65]={0}; int valid=count==1 && hash(get(matched,"sha256"),indexed) && !strcmp(indexed,expected);
    int available=valid && equal(get(matched,"state"),"available");
    xrtValueRelease(root);
    if(!available) { free(folder); return fail(error,valid ? "未找到与目录摘要一致的来源快照。当前资料仅提供已校验的来源，不能查看该文件。" : "来源资料索引无效，不能查看该文件。"); }
    char name[80]; snprintf(name,sizeof(name),"%s.source",expected); char* path=folder ? xidePathResolve(folder,name) : NULL; free(folder);
    bytes=path ? xideIoReadAllLimit(path,4u*1024u*1024u,&size) : NULL; free(path); char actual[65]={0};
    if(!bytes || !xideCatalogDigest(bytes,size,actual) || strcmp(actual,expected)) { xrtFree(bytes); return fail(error,"来源文件缺失或内容已改变；请恢复与命令目录一致的来源资料。"); }
    valid=xideTextFileDecode(bytes,size,file,error); xrtFree(bytes); return valid;
}
