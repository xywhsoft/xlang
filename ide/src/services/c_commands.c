#include "app/features.h"
#if XIDE_ENABLE_C_LSP
#include "services/c_commands.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define C_COMMAND_LIMIT 2048u
static xstrview ccview(const char* s) { return (xstrview){s,strlen(s)}; }
static int ccfail(xide_model_error* e,const char* text) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static int cctext(const char* s,int required)
{ size_t n=s ? strlen(s) : 0; return s && (!required || n) && n<=32768 && (!n || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,(int)n,NULL,0)); }
static int ccstring(xvalue* array,const char* s) { return cctext(s,0) && xrtValueArrayAppendNew(array,xrtValueString(ccview(s))); }
static int ccset(xvalue* object,const char* key,xvalue* v) { return xrtValueObjectSetNew(object,ccview(key),v); }
static int ccinclude(xvalue* args,const char* flag,const char* directory,const char* path)
{ char* resolved=xidePathResolveLexical(directory,path); int ok=resolved && ccstring(args,flag) && ccstring(args,resolved); free(resolved); return ok; }
static int ccadd(xvalue* database,const xvalue* args,const char* base,const char* path)
{
 char* resolved=xidePathResolveLexical(base,path); if(!resolved) return 0;
 for(size_t i=0;i<xrtValueCount(database);++i) { xstrview file={0}; xvalue* entry=xrtValueArrayGet(database,i); if(xrtValueGetString(xrtValueObjectGet(entry,ccview("file")),&file) && xidePathLexicalEqual(file.Data,resolved)) { free(resolved); return 1; } }
 if(xrtValueCount(database)>=C_COMMAND_LIMIT) { free(resolved); return 0; }
 xvalue *entry=xrtValueObject(),*copy=xrtValueDeepClone(args);
 int ok=entry && copy && ccstring(copy,"-c") && ccstring(copy,resolved) && ccset(entry,"directory",xrtValueString(ccview(base))) && ccset(entry,"file",xrtValueString(ccview(resolved))) && xrtValueObjectSet(entry,ccview("arguments"),copy);
 if(ok) ok=xrtValueArrayAppend(database,entry);
 free(resolved); xrtValueRelease(entry); xrtValueRelease(copy); return ok;
}
xvalue* xideCCommandsCreate(const xide_project* project,const char* configurationId,const xide_c_compile_profile* profile,xide_model_error* e)
{
 if(!project || !project->available || !cctext(project->manifest,1) || !project->kind || project->cSources.count>C_COMMAND_LIMIT || project->files.count>C_COMMAND_LIMIT || (profile && (profile->systemIncludeCount>128 || (profile->systemIncludeCount && !profile->systemIncludes)))) { ccfail(e,"Invalid C analysis project/profile or capacity."); return NULL; }
 const xide_configuration* configuration=NULL; const char* id=configurationId ? configurationId : project->activeConfiguration;
 for(size_t i=0;id && i<project->configurationCount;++i) if(!strcmp(id,project->configurations[i].id)) configuration=&project->configurations[i];
 if(!configuration || configuration->includePaths.count>128 || configuration->cAnalysis.systemIncludes.count>128 || configuration->cAnalysis.forcedIncludes.count>128 || configuration->cAnalysis.defines.count>128 || configuration->cAnalysis.undefines.count>128) { ccfail(e,"Choose an existing C analysis configuration."); return NULL; }
 char* base=xidePathDirectory(project->manifest); xvalue *args=xrtValueArray(),*database=xrtValueArray();
 const char* driver=profile && profile->driver ? profile->driver : "clang"; const char* target=profile && profile->target ? profile->target : "x86_64-w64-windows-gnu";
 int ok=base && args && database && cctext(target,1) && cctext(driver,1) && ccstring(args,driver) && ccstring(args,"-x") && ccstring(args,"c") && ccstring(args,"-std=c11") && ccstring(args,"-D_CRT_SECURE_NO_WARNINGS") && ccstring(args,"-target") && ccstring(args,target);
 if(ok && (!strcmp(project->kind,"dll") || !strcmp(project->kind,"shared"))) ok=ccstring(args,"-DXL6_BUILD_SHARED=1");
 const char* defaults[]={".","src","lib/xrt","lib/xruntime"};
 for(size_t i=0;ok && i<4;++i) ok=ccinclude(args,"-I",base,defaults[i]);
 for(size_t i=0;ok && i<configuration->includePaths.count;++i) ok=ccinclude(args,"-I",base,configuration->includePaths.items[i]);
 const xide_c_analysis_config* c=&configuration->cAnalysis;
 for(size_t i=0;ok && i<c->systemIncludes.count;++i) ok=ccinclude(args,"-isystem",base,c->systemIncludes.items[i]);
 for(size_t i=0;ok && i<c->forcedIncludes.count;++i) ok=ccinclude(args,"-include",base,c->forcedIncludes.items[i]);
 const xide_strings* macros[]={&c->defines,&c->undefines}; const char* flags[]={"-D","-U"};
 for(size_t i=0;ok && i<2;++i) for(size_t j=0;ok && j<macros[i]->count;++j) ok=xideCAnalysisMacroValid(macros[i]->items[j],i==0) && ccstring(args,flags[i]) && ccstring(args,macros[i]->items[j]);
 for(size_t i=0;ok && profile && i<profile->systemIncludeCount;++i) ok=ccinclude(args,"-isystem",base,profile->systemIncludes[i]);
 if(ok && profile && profile->prelude) ok=ccinclude(args,"-include",base,profile->prelude);
 ok=ok && xrtValueCount(args)<=1022; /* Session limit includes -c and file. */
 for(size_t i=0;ok && i<project->cSources.count;++i) ok=xidePathHasExtension(project->cSources.items[i],".c") && ccadd(database,args,base,project->cSources.items[i]);
 for(size_t i=0;ok && i<project->files.count;++i) if(!strcmp(project->files.items[i].kind,"header")) ok=xidePathHasExtension(project->files.items[i].path,".h") && ccadd(database,args,base,project->files.items[i].path);
 size_t bytes=0; char* serialized=ok ? xrtJsonStringify(database,false,&bytes) : NULL; ok=serialized && bytes<=4u*1024u*1024u; xrtFree(serialized);
 free(base); xrtValueRelease(args); if(!ok) { xrtValueRelease(database); ccfail(e,"Cannot construct bounded C compilation commands."); return NULL; } return database;
}
int xideCCommandsAddContextFile(xvalue* database,const char* origin,const char* path,xide_model_error* e)
{
 if(!database || !cctext(origin,1) || !cctext(path,1)) return ccfail(e,"Invalid C navigation context.");
 const xvalue* row=NULL;
 for(size_t i=0;i<xrtValueCount(database);++i) {
  const xvalue* candidate=xrtValueArrayGet(database,i); xstrview file={0};
  if(xrtValueGetString(xrtValueObjectGet(candidate,ccview("file")),&file) && xidePathLexicalEqual(file.Data,origin)) { row=candidate; break; }
 }
 const xvalue* original=row ? xrtValueObjectGet(row,ccview("arguments")) : NULL; size_t count=xrtValueCount(original); xstrview directory={0},option={0},operand={0};
 if(!row || count<2 || !xrtValueGetString(xrtValueObjectGet(row,ccview("directory")),&directory) ||
    !xrtValueGetString(xrtValueArrayGet(original,count-2),&option) || strcmp(option.Data,"-c") ||
    !xrtValueGetString(xrtValueArrayGet(original,count-1),&operand) || !xidePathLexicalEqual(operand.Data,origin)) return ccfail(e,"C navigation origin has no current project command.");
 xvalue* args=xrtValueArray(); int okay=args!=NULL;
 for(size_t i=0;okay && i<count-2;++i) okay=xrtValueArrayAppend(args,xrtValueArrayGet(original,i));
 okay=okay && ccadd(database,args,directory.Data,path); xrtValueRelease(args);
 size_t size=0; char* encoded=okay ? xrtJsonStringify(database,false,&size) : NULL; okay=encoded && size<=4u*1024u*1024u; xrtFree(encoded);
 return okay ? 1 : ccfail(e,"Cannot add a bounded C navigation command.");
}

#endif /* XIDE_ENABLE_C_LSP */
