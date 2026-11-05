/* Offline observer only; no compiler object enters the timed programs. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include <stdio.h>
static void quoted(const char* text)
{
 putchar('"');for(;*text;++text){if(*text=='"'||*text=='\\')putchar('\\');putchar(*text);}putchar('"');
}
int main(int argc,char** argv)
{
 if(argc!=3)return 2;
 XL6_CompileUnit unit;XL6_ArtifactRuntimePlan plan;char* native=NULL;
 xl6CompileUnitInit(&unit);
 int ok=xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags)&&
  xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags)&&
  xl6ArtifactRuntimeRequireSymbols(&plan,(const char* const*)unit.importDeps.runtimeSymbols.items,unit.importDeps.runtimeSymbols.count)&&
  xl6XrtTrimBuildImplementationSourceFromPlan(&unit.importDeps,0,&plan,&native);
 if(ok){FILE* file=fopen(argv[2],"wb");ok=file && fputs(native,file)>=0;if(file&&fclose(file))ok=0;}
 if(ok){
  fputs("{\"sources\":[",stdout);
  for(size_t i=0;i<plan.sourceCount;++i){if(i)putchar(',');quoted(plan.sources[i]);}
  fputs("],\"xrtSources\":[",stdout);
  for(size_t i=0;i<plan.xrtSourceCount;++i){if(i)putchar(',');quoted(plan.xrtSources[i]);}
  fputs("]",stdout);
#ifdef BINDING_PERF_SEQUENCE
  fputs(",\"schemas\":[",stdout);int first=1;
  for(size_t i=0;i<unit.ir.types.count;++i){
   XL6_IrTypeId element=xl6IrNativeSequenceElement(&unit.ir,(XL6_IrTypeId)i+1);if(!element)continue;
   const XL6_IrType* item=xl6IrTypeGet(&unit.ir.types,element);if(!first)putchar(',');first=0;
   fputs("{\"elementCName\":",stdout);quoted(item->cName);
   printf(",\"sequenceIdentity\":%llu,\"elementIdentity\":%llu}",
    (unsigned long long)unit.ir.types.items[i].typeValueId,(unsigned long long)item->typeValueId);
  }
  putchar(']');
#endif
  puts("}");
 }else xl6DiagPrintText(&unit.parse.diags,argv[1]);
 xrtFree(native);xl6CompileUnitFree(&unit);return ok?0:1;
}
