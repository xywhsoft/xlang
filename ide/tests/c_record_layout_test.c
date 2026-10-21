/* Exercise optional configured-layout metadata at the real bridge boundary. */
#include "../src/services/c_bridge.c"
static int checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"c_record_layout_test:%d: %s\n",__LINE__,#v); failed=1; goto cleanup; } } while(0)
static int has(const xvalue* args,const char* text)
{ for(size_t i=0;i<xrtValueCount(args);++i) if(!strcmp(cbstr(xrtValueArrayGet(args,i),128),text)) return 1; return 0; }
static xvalue* scalar(void)
{
 xvalue* p=xrtValueObject();
 if(!p || !cbput(p,"schema",xrtValueInt(1)) || !cbput(p,"scope",xrtValueString(cbsv("tcc-win64-scalar-target"))) ||
    !cbput(p,"tinyCVersion",xrtValueInt(927)) || !cbput(p,"pointerBytes",xrtValueInt(8)) ||
    !cbput(p,"longBytes",xrtValueInt(4)) || !cbput(p,"longDoubleBytes",xrtValueInt(8)) || !cbput(p,"longDoubleAlign",xrtValueInt(8))) { xrtValueRelease(p); return NULL; }
 return p;
}
static xvalue* layout(int microsoft)
{
 xvalue* p=xrtValueObject();
 if(!p || !cbput(p,"schema",xrtValueInt(1)) || !cbput(p,"scope",xrtValueString(cbsv("tcc-win64-bitfield-default"))) ||
    !cbput(p,"msBitfields",xrtValueBool(microsoft!=0))) { xrtValueRelease(p); return NULL; }
 return p;
}
int main(void)
{
 int failed=0; xvalue *environment=xrtValueObject(),*args=xrtValueArray(),*old=xrtValueArray(),*bad=NULL,*before=NULL,*database=NULL,*project=NULL;
 CHECK(environment && args && old && cbarg(old,"clang") && cbarg(old,"-target") && cbarg(old,"x86_64-w64-windows-gnu"));
 CHECK(cbrecordTarget(args,environment) && xrtValueCount(args)==0);
 CHECK(cbput(environment,"recordLayout",layout(0)) && !cbrecordTarget(args,environment) && xrtValueCount(args)==0);
 CHECK(cbput(environment,"scalarTarget",scalar()) && cbscalarTarget(args,old,environment) && cbrecordTarget(args,environment));
 CHECK(has(args,"-mno-ms-bitfields") && !has(args,"-mms-bitfields") && has(args,"-mlong-double-64"));
 xrtValueRelease(args);args=xrtValueArray();CHECK(args && cbput(environment,"recordLayout",layout(1)) && cbrecordTarget(args,environment));
 CHECK(has(args,"-mms-bitfields") && !has(args,"-mno-ms-bitfields"));
 for(int variant=0;variant<4;++variant) {
  bad=xrtValueDeepClone(environment);CHECK(bad);xvalue* p=(xvalue*)cbget(bad,"recordLayout");
  if(variant==0) CHECK(cbput(p,"schema",xrtValueInt(2)));
  else if(variant==1) CHECK(cbput(p,"scope",xrtValueString(cbsv("tcc-other-target"))));
  else if(variant==2) CHECK(cbput(p,"msBitfields",xrtValueInt(1)));
  else CHECK(cbput(p,"msBitfields",xrtValueNull()));
  before=xrtValueDeepClone(args);CHECK(before && !cbrecordTarget(args,bad) && xrtValueEqual(args,before));
  xrtValueRelease(bad);bad=NULL;xrtValueRelease(before);before=NULL;
 }
 /* User analysis arguments live in the original compilation command, after
    the configured native defaults and the restricted compiler operations. */
 database=xrtJsonParse(cbsv("[{\"file\":\"C:/native.c\",\"arguments\":[\"clang\",\"-target\",\"x86_64-w64-windows-gnu\",\"-mms-bitfields\",\"-x\",\"c\",\"C:/native.c\"]}]"));
 project=xrtJsonParse(cbsv("{\"directory\":\"C:/\",\"files\":[\"C:/native.c\"]}"));
 CHECK(database && project && cbput(environment,"recordLayout",layout(0)) && cbput(environment,"operations",xrtValueArray()));
 CHECK(cbapply(database,project,environment,"C:/bridge.h","C:/bridge.vfs.json"));
 const xvalue* prepared=cbget(xrtValueArrayGet(database,0),"arguments");size_t nativeIndex=SIZE_MAX,userIndex=SIZE_MAX;
 for(size_t i=0;i<xrtValueCount(prepared);++i) {
  const char* argument=cbstr(xrtValueArrayGet(prepared,i),128);
  if(!strcmp(argument,"-mno-ms-bitfields")) nativeIndex=i;
  if(!strcmp(argument,"-mms-bitfields")) userIndex=i;
 }
 CHECK(nativeIndex!=SIZE_MAX && userIndex!=SIZE_MAX && nativeIndex<userIndex && has(prepared,"-target"));
 printf("c_record_layout_test: %d checks, passed; optional defaults, strict metadata and user precedence.\n",checks);
cleanup:
 xrtValueRelease(environment);xrtValueRelease(args);xrtValueRelease(old);xrtValueRelease(bad);xrtValueRelease(before);xrtValueRelease(database);xrtValueRelease(project);return failed;
}
