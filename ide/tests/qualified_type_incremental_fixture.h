#ifndef XLS_QUALIFIED_TYPE_INCREMENTAL_FIXTURE_H
#define XLS_QUALIFIED_TYPE_INCREMENTAL_FIXTURE_H
#include "analysis/check_session.h"
static char* qualifiedIr(const xls_semantic_check* check)
{
 const XL6_IrModule* ir=xlsSemanticCheckIr(check);if(!ir)return NULL;XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;
 xl6IrDumpModuleJson(&out,ir);char* text=xl6OutputHasFailed(&out)?NULL:xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;
}
static int qualifiedFacts(xls_semantic_check* check,const char* entry,const char* source,const char* dep,const char* library,const char* name,const char* spelling)
{
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);CHECK(index);const char* begin=strstr(source,"::<");CHECK(begin);begin+=3;
 size_t offset=(size_t)(begin-source)-3;const char* declared=strstr(library,name);CHECK(declared);unsigned reference=0,declaration=0;uint64_t a=0,b=0;size_t references=0;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact fact;CHECK(xlsSemanticIndexFact(index,i,&fact));xls_input_source file={0};
  if(fact.source)CHECK(xlsSemanticCheckSource(check,fact.source-1,&file));
  if(fact.source && !strcmp(file.path,entry) && fact.referencedType && fact.tokenRange.startOffset==offset){
   CHECK(fact.tokenRange.known && fact.tokenRange.endOffset==offset+strlen(spelling));reference=fact.referencedType;a=fact.typeAbi;++references;
  }
  if(fact.declaredType && fact.selectionSource){CHECK(xlsSemanticCheckSource(check,fact.selectionSource-1,&file));if(!strcmp(file.path,dep) && fact.selectionRange.startOffset==(size_t)(declared-library)){declaration=fact.declaredType;b=fact.typeAbi;}}
 }
 CHECK(references==1 && reference && reference==declaration && a && a==b);return 1;
}
static int qualifiedIncremental(void)
{
#define Q_PREFIX "\xef\xbb\xbf" "import dep\r\nfunc stable(int value) as int\r\n return value\r\nend\r\nfunc main(...) as int\r\n"
#define Q_END "\r\n return 0\r\nend\r\n"
 const char* variants[]={
  Q_PREFIX " auto box=dep.Box::<dep.Marker>(dep.Marker())" Q_END,
  Q_PREFIX " string note=\"😀\"\r\n auto box=dep.Box::<dep.Marker>(dep.Marker())" Q_END,
  Q_PREFIX " string note=\"😀\"\r\n auto box=dep.Box::<dep  .  Marker>(dep.Marker())" Q_END,
  Q_PREFIX " auto box=dep.Box::<dep.Other>(dep.Other())" Q_END,
  Q_PREFIX " auto box=dep.Box::<dep.Missing>(dep.Other())" Q_END,
  Q_PREFIX " auto box=dep.Box::<dep.Other>(dep.Other())" Q_END,
 };
#undef Q_PREFIX
#undef Q_END
 const char* library="export class Box<T>\n T value\n create(T value)\n self.value=value\n end\nend\nexport class Marker\nend\nexport class Other\nend\n";
 char* directory=xrtPathAbs("build/qualified-type-incremental");CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"入口😀.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(entry && dep);
 xls_check_session* session=xlsCheckSessionCreate(NULL);xls_text* lib=xlsTextCreate(library,strlen(library),1024*1024,NULL);CHECK(session && lib);xls_semantic_check* held=NULL;
 for(size_t i=0;i<sizeof(variants)/sizeof(*variants);++i){const char* source=variants[i];xls_text* text=xlsTextCreate(source,strlen(source),1024*1024,NULL);CHECK(text);
  xls_analysis_document documents[]={{"entry",entry,text,1,i+1},{"dep",dep,lib,2,1}};
  xls_semantic_check* actual=xlsCheckSessionCheck(session,documents,2,"entry",1,NULL,NULL,NULL,0);CHECK(actual && xlsSemanticCheckInfo(actual).accepted==(i!=4));
  xls_semantic_input inputs[]={{entry,source,strlen(source)},{dep,library,strlen(library)}};xls_semantic_check* fresh=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);CHECK(fresh);
  char *a=xlsSemanticCheckJson(actual,NULL),*b=xlsSemanticCheckJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  if(i!=4){const char *name=i<3?"Marker":"Other",*spelling=i==2?"dep  .  Marker":i<3?"dep.Marker":"dep.Other";
   CHECK(qualifiedFacts(actual,entry,source,dep,library,name,spelling));CHECK(!strcmp(xlsSemanticCheckIndex(actual),xlsSemanticCheckIndex(fresh)));
   a=qualifiedIr(actual);b=qualifiedIr(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
  }else CHECK(!xlsSemanticCheckTypedIndex(actual));
  xls_semantic_check* hit=xlsCheckSessionCheck(session,documents,2,"entry",1,NULL,NULL,NULL,0);CHECK(hit==actual);xlsSemanticCheckDestroy(hit);
  if(!i)held=xlsSemanticCheckRetain(actual);
  xlsSemanticCheckDestroy(actual);xlsSemanticCheckDestroy(fresh);xlsTextRelease(text);
 }
 xls_check_stats stats={0};xlsCheckSessionStats(session,&stats);CHECK(stats.builds==6 && stats.hits==6 && stats.entryFragmentParses && stats.entryReusedDeclarations);
 printf("qualified incremental: builds=%llu hits=%llu fragments=%llu reusedDeclarations=%llu\n",(unsigned long long)stats.builds,(unsigned long long)stats.hits,(unsigned long long)stats.entryFragmentParses,(unsigned long long)stats.entryReusedDeclarations);
 xlsCheckSessionDestroy(session);CHECK(held && qualifiedFacts(held,entry,variants[0],dep,library,"Marker","dep.Marker"));xlsSemanticCheckDestroy(held);xlsTextRelease(lib);xrtFree(entry);xrtFree(dep);xrtFree(directory);return 1;
}
#endif
