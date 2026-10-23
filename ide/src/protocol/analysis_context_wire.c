#include "protocol/analysis_context_wire.h"
#include <stdio.h>
#include <string.h>
static const char* fields[XLS_CONTEXT_LIST_COUNT]={"members","cSources","resources","includes","libraryPaths","linkLibraries","cSystemIncludes","cForcedIncludes","cDefines","cUndefines"};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* v,const char* name) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(name)) : NULL; }
static const char* text(const xvalue* value)
{ xstrview s={0};return xrtValueGetString(value,&s) && s.Size<=262144 && (!s.Size || (s.Data && !memchr(s.Data,0,s.Size))) ? (s.Data ? s.Data : "") : NULL; }
static xls_context* fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message);return NULL; }
static int put(xvalue* v,const char* name,xvalue* owned) { return owned && xrtValueObjectSetNew(v,sv(name),owned); }
xls_context* xideAnalysisContextRead(const xvalue* value,char* error,size_t capacity)
{
 if(error && capacity) *error=0;
 int64 schema=0;const char* kind=text(get(value,"kind"));const xvalue* lists=get(value,"lists"),*package=get(value,"packagePath");
 if(xrtValueType(value)!=XVALUE_OBJECT || xrtValueCount(value)!=10 || !xrtValueGetInt(get(value,"schema"),&schema) || schema!=1 || !kind ||
    xrtValueType(lists)!=XVALUE_OBJECT || xrtValueCount(lists)!=XLS_CONTEXT_LIST_COUNT || !package)
  return fail(error,capacity,"Invalid analysis context schema.");
 xls_context_spec spec={.kind=!strcmp(kind,"project") ? XLS_CONTEXT_PROJECT : !strcmp(kind,"xlang") ? XLS_CONTEXT_XLANG : !strcmp(kind,"c") ? XLS_CONTEXT_C : 0,
  .project=text(get(value,"project")),.configuration=text(get(value,"configuration")),.entry=text(get(value,"entry")),
  .directory=text(get(value,"directory")),.analysisProfile=text(get(value,"analysisProfile")),.targetKind=text(get(value,"targetKind")),
  .packagePath=xrtValueType(package)==XVALUE_NULL ? NULL : text(package)};
 if(!spec.kind || !spec.project || !spec.configuration || !spec.entry || !spec.directory || !spec.analysisProfile || !spec.targetKind ||
    (xrtValueType(package)!=XVALUE_NULL && !spec.packagePath)) return fail(error,capacity,"Invalid analysis context strings.");
 const char** pointers[XLS_CONTEXT_LIST_COUNT]={0};size_t total=0;xls_context* result=NULL;
 for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) {
  const xvalue* list=get(lists,fields[i]);size_t count=xrtValueCount(list);
  if(xrtValueType(list)!=XVALUE_ARRAY || count>65536-total) goto invalid;
  total+=count;
  if(count && !(pointers[i]=xrtCalloc(count,sizeof(char*)))) goto memory;
  for(size_t j=0;j<count;++j) if(!(pointers[i][j]=text(xrtValueArrayGet(list,j)))) goto invalid;
  spec.lists[i]=(xls_context_strings){pointers[i],count};
 }
 enum xls_context_error status;result=xlsContextCreate(&spec,16u*1024u*1024u,&status);
 if(!result) fail(error,capacity,status==XLS_CONTEXT_MEMORY ? "Cannot allocate analysis context." : "Analysis context capacity or encoding is invalid.");
 goto done;
invalid:fail(error,capacity,"Invalid analysis context list or capacity.");goto done;
memory:fail(error,capacity,"Cannot decode analysis context lists.");
done:for(size_t i=0;i<XLS_CONTEXT_LIST_COUNT;++i) xrtFree(pointers[i]);return result;
}
xvalue* xideAnalysisContextWrite(const xls_context* context)
{
 if(!context) return NULL;
 const xls_context_spec* s=xlsContextSpec(context);xvalue *root=xrtValueObject(),*lists=xrtValueObject();
 const char* kind=s->kind==XLS_CONTEXT_PROJECT ? "project" : s->kind==XLS_CONTEXT_XLANG ? "xlang" : "c";
 int okay=root && lists && put(root,"schema",xrtValueInt(1)) && put(root,"kind",xrtValueString(sv(kind))) &&
  put(root,"project",xrtValueString(sv(s->project))) && put(root,"configuration",xrtValueString(sv(s->configuration))) &&
  put(root,"entry",xrtValueString(sv(s->entry))) && put(root,"directory",xrtValueString(sv(s->directory))) &&
  put(root,"analysisProfile",xrtValueString(sv(s->analysisProfile))) && put(root,"targetKind",xrtValueString(sv(s->targetKind))) &&
  put(root,"packagePath",s->packagePath ? xrtValueString(sv(s->packagePath)) : xrtValueNull());
 for(size_t i=0;okay && i<XLS_CONTEXT_LIST_COUNT;++i) {
  xvalue* list=xrtValueArray();okay=list!=NULL;
  for(size_t j=0;okay && j<s->lists[i].count;++j) {xvalue* item=xrtValueString(sv(s->lists[i].items[j]));okay=item && xrtValueArrayAppend(list,item);xrtValueRelease(item);}
  if(okay) {okay=put(lists,fields[i],list);list=NULL;}xrtValueRelease(list);
 }
 if(okay) {okay=put(root,"lists",lists);lists=NULL;}xrtValueRelease(lists);
 if(!okay) {xrtValueRelease(root);root=NULL;}return root;
}
