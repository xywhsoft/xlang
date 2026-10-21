#include "services/library_info.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define MANIFEST_LIMIT (4u*1024u*1024u)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) ? s.Data : ""; }
static int validString(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=4096 && !memchr(s.Data,0,s.Size); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,const char* s) { if(e) { e->code=XIDE_MODEL_IO; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
void xideLibraryInfoClear(xide_library_info* info) { if(info) { free(info->manifest); free(info->version); free(info->abiVersion); memset(info,0,sizeof(*info)); } }
size_t xideLibraryCatalogEntry(const xide_command_catalog* catalog,const char* name)
{
    if(!catalog || !name) return SIZE_MAX;
    for(size_t i=0;i<catalog->count;++i) { const xide_command* c=&catalog->entries[i];
        if(!strcmp(c->language,"xlang") && !strcmp(c->category,"extension") && !strcmp(c->kind,"module") && !strcmp(c->name,name)) return i;
    }
    return SIZE_MAX;
}
static char* candidate(const char* base,const char* name)
{
    char* dir=xidePathResolve(base,name); char* file=dir ? xidePathResolve(dir,"xlang.package.json") : NULL; free(dir);
    if(file && xidePathExists(file)) return file;
    free(file); return NULL;
}
static int named(const char* name) { return name && *name && *name!='.' && !strpbrk(name,"/\\:"); }
static char* locate(const xide_project* project,const char* name,const char* compiler,const char* packagePath)
{
    char *entryDir=xidePathDirectory(project->entry),*projectDir=xidePathDirectory(project->manifest),*found=NULL;
    if(named(name)) {
        found=entryDir ? candidate(entryDir,name) : NULL;
        for(const char* start=packagePath ? packagePath : "";!found && *start;) {
            const char* end=strchr(start,';'); size_t n=end ? (size_t)(end-start) : strlen(start);
            char* raw=malloc(n+1); if(!raw) break; memcpy(raw,start,n); raw[n]=0;
            char* base=n ? xidePathResolve(projectDir,raw) : NULL; free(raw);
            found=base ? candidate(base,name) : NULL; free(base); if(!end) break; start=end+1;
        }
        char* exe=compiler && *compiler ? xidePathResolve(NULL,compiler) : NULL; char* dir=exe ? xidePathDirectory(exe) : NULL; free(exe);
        for(int depth=0;!found && dir && depth<128;++depth) {
            char* libs=xidePathResolve(dir,"extlibs"); found=libs ? candidate(libs,name) : NULL; free(libs);
            char* parent=xidePathDirectory(dir); if(!parent || !strcmp(parent,dir)) { free(parent); break; }
            free(dir); dir=parent;
        }
        free(dir);
    }
    if(!found && strpbrk(name,"/\\:.")) {
        char* path=entryDir ? xidePathResolve(entryDir,name) : NULL;
        if(path && xidePathExists(path) && !xidePathIsDirectory(path)) found=path;
        else { found=path ? candidate(path,".") : NULL; free(path); }
        if(!found) { path=xidePathResolve(projectDir,name);
            if(path && xidePathExists(path) && !xidePathIsDirectory(path)) found=path;
            else { found=path ? candidate(path,".") : NULL; free(path); }
        }
    }
    free(entryDir); free(projectDir); return found;
}
static int files(xide_library_info* info,const xvalue* owner,const char* key,const char* base)
{
    xvalue* values=get(owner,key); if(!values) return 1;
    if(xrtValueType(values)!=XVALUE_ARRAY || xrtValueCount(values)>4096-info->declaredFiles) return 0;
    for(size_t i=0;i<xrtValueCount(values);++i) {
        xvalue* value=xrtValueArrayGet(values,i); if(!validString(value) || !*text(value)) return 0;
        char* path=xidePathResolve(base,text(value)); xfileinfo stat={0}; ++info->declaredFiles;
        if(!path || !xrtPathStat(path,true,&stat) || stat.Type!=XFILE_TYPE_FILE) ++info->missingFiles;
        free(path);
    }
    return 1;
}
int xideLibraryInspect(xide_library_info* out,const xide_project* project,const char* kind,const char* name,const char* compiler,const char* packagePath,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!out || !project || !project->manifest || !project->entry || !kind || !name || !*name) return fail(error,"Library inspection requires a project and reference name.");
    xide_library_info info={0};
    if(strcmp(kind,"package")) { info.state=XIDE_LIBRARY_UNSUPPORTED; snprintf(info.message,sizeof(info.message),"此库种类保留为工程元数据，尚无检查适配器。"); }
    else {
        info.manifest=locate(project,name,compiler,packagePath);
        if(!info.manifest) snprintf(info.message,sizeof(info.message),"未找到磁盘包清单；内置模块和其他编译器解析方式仍需实际构建确认。");
        else {
            size_t length=0; bytes data=xrtFileReadAllLimit(info.manifest,MANIFEST_LIMIT,&length); const unsigned char* body=data;
            if(data && length>=3 && !memcmp(data,"\xef\xbb\xbf",3)) { body+=3; length-=3; }
            xvalue* root=data && !memchr(body,0,length) ? xrtJsonParse((xstrview){(const char*)body,length}) : NULL;
            xvalue* version=get(root,"version"),*abi=get(root,"abiVersion"),*artifact=get(root,"artifact");
            int valid=root && validString(get(root,"format")) && !strcmp(text(get(root,"format")),"xlang-package-1") && (!get(root,"name") || validString(get(root,"name"))) && (!version || validString(version)) && (!abi || validString(abi)) && (!artifact || xrtValueType(artifact)==XVALUE_OBJECT);
            char* base=xidePathDirectory(info.manifest);
            if(valid) valid=base && files(&info,artifact,"xlSources",base) && files(&info,artifact,"cSources",base) && files(&info,root,"xlSources",base) && files(&info,root,"cSources",base);
            if(valid) {
                info.version=copy(version ? text(version) : "0.0.0"); info.abiVersion=copy(abi ? text(abi) : "1");
                if(!info.version || !info.abiVersion) { free(base); xrtValueRelease(root); xrtFree(data); xideLibraryInfoClear(&info); return fail(error,"Cannot allocate package metadata."); }
                info.state=info.missingFiles ? XIDE_LIBRARY_INCOMPLETE : XIDE_LIBRARY_FOUND;
                snprintf(info.message,sizeof(info.message),"%s；声明文件 %zu 项，缺失 %zu 项。链接、原生 DLL 和递归依赖由实际构建确认。",info.missingFiles ? "候选包文件不完整" : "候选清单可读",info.declaredFiles,info.missingFiles);
            } else { info.state=XIDE_LIBRARY_INVALID; snprintf(info.message,sizeof(info.message),"首个磁盘清单不可读或字段格式不支持；编译器可能继续查找其他候选。单个清单上限 4 MiB。"); }
            free(base); xrtValueRelease(root); xrtFree(data);
        }
    }
    xideLibraryInfoClear(out); *out=info; return 1;
}
