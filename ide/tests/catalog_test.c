#include "services/command_catalog.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"catalog_test:%d: %s; %s\n",__LINE__,#expr,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xide_command* findName(xide_command_catalog* catalog,const char* name)
{
    for(size_t i=0;i<catalog->count;++i) if(!strcmp(catalog->entries[i].name,name)) return &catalog->entries[i];
    return NULL;
}
static int save(const char* path,xvalue* value)
{
    size_t size; char* text=xrtJsonStringify(value,true,&size); int result=text && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,size}); xrtFree(text); return result;
}
int main(int argc,char** argv)
{
    int failed=0; xide_model_error error={0}; xide_command_catalog catalog={0}; char *root=NULL,*path=NULL; xvalue* bad=NULL;
    CHECK(argc==3); root=xidePathResolve(argv[1],"中文 命令目录"); CHECK(root && xrtDirCreateAll(root)); path=xidePathResolve(root,"invalid.json"); CHECK(path);
    CHECK(xideCatalogLoad(&catalog,argv[2],&error)); CHECK(catalog.count>4400 && catalog.documentCount>=6 && xideCatalogDocument(&catalog,"package:csv"));
    const xide_command* jsonFinish=findName(&catalog,"JsonWriter.finish"),*xlonFinish=findName(&catalog,"XlonWriter.finish"),*fixedStack=findName(&catalog,"FixedStack.tryPop"),*genericXlon=xideCatalogFind(&catalog,"standard:xlon.stringify:9b21a2969175e238");
    CHECK(jsonFinish && !strcmp(jsonFinish->signature,"JsonWriter.finish() as string") && xlonFinish && !strcmp(xlonFinish->signature,"XlonWriter.finish() as string"));
    CHECK(fixedStack && strstr(fixedStack->signature,"(bool, T?)") && genericXlon && strstr(genericXlon->signature,"<T>"));
    const xide_command* type=xideCatalogFind(&catalog,"language:type:dict"); CHECK(type && !strcmp(type->signature,"dict<T1>") && strstr(type->source,"xl6_type_registry.c") && type->sourceLine>1);
    CHECK(type->documentCount==1 && type->documents[0]==xideCatalogDocument(&catalog,"design:类型系统设计"));
    CHECK(xideCatalogFind(&catalog,"language:func")->documentCount==1 && strstr(xideCatalogFind(&catalog,"language:func")->documents[0]->markdown,"普通函数声明"));
    CHECK(xideCatalogFind(&catalog,"language:func")==&catalog.entries[0]); CHECK(xideCatalogFind(&catalog,"language:class")==&catalog.entries[1]);
    CHECK(!findName(&catalog,"__xl_runtime.print"));
    CHECK(xideCatalogFind(&catalog,"c:keyword:inline") && xideCatalogFind(&catalog,"c:keyword:restrict"));
    const xide_command* sqrt=findName(&catalog,"math.sqrt"); CHECK(sqrt && sqrt->parameterCount==1 && !strcmp(sqrt->parameters[0].name,"x") && !strcmp(sqrt->parameters[0].type,"float"));
    CHECK(!strcmp(sqrt->returns,"float") && !strcmp(sqrt->signature,"math.sqrt(float x) as float"));
    CHECK(xideCatalogMatches(sqrt,"MATH sqrt float") && !xideCatalogMatches(sqrt,"math impossible"));
    CHECK(xideCatalogCanComplete(sqrt,"xlang") && !xideCatalogCanComplete(sqrt,"c") && !xideCatalogCanComplete(sqrt,"text"));
    const xide_command* split=findName(&catalog,"string.split"); CHECK(split && split->parameterCount==4 && split->parameters[2].optional && !strcmp(split->parameters[2].defaultValue,"-1"));
    CHECK(findName(&catalog,"log.printf") && !strcmp(findName(&catalog,"log.printf")->parameters[2].type,"c_vararg"));
    const xide_command* module=findName(&catalog,"csv"); CHECK(module && !strcmp(module->requiredImport,"csv") && !strcmp(module->version,"1.0.0") && !xideCatalogCanComplete(module,"xlang"));
    const xide_command* reader=findName(&catalog,"csv.Reader"); CHECK(reader && !strcmp(reader->kind,"type"));
    CHECK(findName(&catalog,"xacme") && !findName(&catalog,"acme") && !findName(&catalog,"_bzip2"));
    const xide_command* zstdLevel=findName(&catalog,"zstd.ZSTD_DEFAULT_LEVEL");
    CHECK(zstdLevel && !strcmp(zstdLevel->kind,"constant") && strstr(zstdLevel->signature,"= 3"));
    const xide_command* bloomType=findName(&catalog,"bloom.BloomFilter");
    CHECK(bloomType && !xideCatalogCanComplete(bloomType,"xlang") && strstr(bloomType->description,"未直接导出"));
    CHECK(findName(&catalog,"bloom.BloomFilter.contains") && !findName(&catalog,"bloom.BloomFilter.handle"));
    size_t fields=0;
    size_t c=0,methods=0,extensions=0;
    for(size_t i=0;i<catalog.count;++i) { const xide_command* command=&catalog.entries[i]; if(!strcmp(command->language,"c")) { ++c; CHECK(!strcmp(command->category,"language") && !*command->requiredImport); } if(!strcmp(command->kind,"method")) { ++methods; CHECK(!xideCatalogCanComplete(command,"xlang")); } if(!strcmp(command->category,"extension")) { ++extensions; CHECK(*command->requiredImport && *command->source && command->sourceLine>=1); } }
    for(size_t i=0;i<catalog.count;++i) if(!strcmp(catalog.entries[i].kind,"field")) { ++fields; CHECK(!xideCatalogCanComplete(&catalog.entries[i],"xlang")); }
    CHECK(c==44 && methods>2000 && extensions>2000 && fields>0);
    const xide_command* original=catalog.entries; size_t count=catalog.count;
    CHECK(!xideCatalogLoad(&catalog,"ide/build/not-a-catalog.json",&error) && catalog.entries==original && catalog.count==count);
    bad=xrtValueDeepClone(catalog.source); CHECK(bad); xvalue* entries=xrtValueObjectGet(bad,sv("entries")); xvalue* first=xrtValueArrayGet(entries,0);
    CHECK(xrtValueObjectSetNew(first,sv("language"),xrtValueString(sv("invented"))) && save(path,bad));
    CHECK(!xideCatalogLoad(&catalog,path,&error) && error.code==XIDE_MODEL_INVALID && catalog.entries==original); xrtValueRelease(bad); bad=xrtValueDeepClone(catalog.source); CHECK(bad);
    entries=xrtValueObjectGet(bad,sv("entries")); first=xrtValueArrayGet(entries,1); CHECK(xrtValueObjectSetNew(first,sv("id"),xrtValueString(sv("language:func"))) && save(path,bad));
    CHECK(!xideCatalogLoad(&catalog,path,&error) && error.code==XIDE_MODEL_CONFLICT && catalog.entries==original); xrtValueRelease(bad); bad=xrtValueDeepClone(catalog.source); CHECK(bad);
    entries=xrtValueObjectGet(bad,sv("entries")); first=xrtValueArrayGet(entries,0);
    xvalue* refs=xrtValueArray(); CHECK(refs && xrtValueArrayAppendNew(refs,xrtValueString(sv("missing:document"))) && xrtValueObjectSetNew(first,sv("sections"),refs) && save(path,bad));
    CHECK(!xideCatalogLoad(&catalog,path,&error) && catalog.entries==original); xrtValueRelease(bad); bad=xrtValueDeepClone(catalog.source); CHECK(bad);
    xvalue* docs=xrtValueObjectGet(bad,sv("documents")); first=xrtValueArrayGet(docs,1);
    CHECK(xrtValueObjectSetNew(first,sv("id"),xrtValueString(sv("design:语言基础设计"))) && save(path,bad));
    CHECK(!xideCatalogLoad(&catalog,path,&error) && catalog.entries==original);
    const unsigned char invalidJson[]={'{','"','d','"',':','"',0xff,'"','}'};
    CHECK(xrtFileWriteAtomic(path,(xbytesview){invalidJson,sizeof(invalidJson)})); CHECK(!xideCatalogLoad(&catalog,path,&error) && catalog.entries==original);
    puts("catalog_test passed: real builtin types/shared document refs, signatures/defaults, public package metadata, C isolation, member exclusions, UTF-8, duplicate IDs and transactional failures without a UI context");
cleanup:
    xrtValueRelease(bad); xideCatalogClear(&catalog); free(path); free(root); return failed;
}
