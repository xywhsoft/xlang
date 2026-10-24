#include "protocol/signature_help.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"signature_help_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key){return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,sv(key)) : NULL;}
static int context(const char* json,int expected)
{xvalue* value=xrtJsonParse(sv(json));CHECK(value);CHECK(xlsSignatureHelpContextValid(value)==expected);xrtValueRelease(value);return 1;}
static int capability(const char* json,int expected,unsigned flags)
{xvalue* value=xrtJsonParse(sv(json));unsigned actual=99;CHECK(value);CHECK(xlsSignatureHelpCapabilities(value,&actual)==expected);if(expected)CHECK(actual==flags);xrtValueRelease(value);return 1;}
static int integer(const xvalue* v,int64 expected){int64 n=-1;return xrtValueGetInt(v,&n) && n==expected;}
static int run(void)
{
 CHECK(xlsSignatureHelpContextValid(NULL));
 CHECK(context("{}",0));CHECK(context("null",0));CHECK(context("[]",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false}",1));
 CHECK(context("{\"triggerKind\":2,\"triggerCharacter\":\"(\",\"isRetrigger\":false}",1));
 CHECK(context("{\"triggerKind\":3,\"isRetrigger\":true}",1));
 CHECK(context("{\"triggerKind\":2,\"triggerCharacter\":\"😀\",\"isRetrigger\":true}",1));
 CHECK(context("{\"triggerKind\":2,\"isRetrigger\":false}",0));
 CHECK(context("{\"triggerKind\":0,\"isRetrigger\":false}",0));
 CHECK(context("{\"triggerKind\":4,\"isRetrigger\":false}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":0}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"triggerCharacter\":\"ab\"}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":null}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":{\"signatures\":[]}}",1));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":{\"signatures\":[{\"label\":\"😀(int)\",\"parameters\":[{\"label\":[3,6]}]}]}}",1));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":{\"signatures\":[{\"label\":\"f(int)\",\"parameters\":[{\"label\":[2,99]}]}]}}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":{\"signatures\":[{\"label\":\"f(int)\",\"parameters\":[{\"label\":[4,2]}]}]}}",0));
 CHECK(context("{\"triggerKind\":1,\"isRetrigger\":false,\"activeSignatureHelp\":{\"signatures\":[{\"label\":\"f(int)\",\"documentation\":{\"kind\":\"html\",\"value\":\"bad\"}}]}}",0));
 CHECK(capability("{}",1,0));
 CHECK(capability("{\"textDocument\":{\"signatureHelp\":false}}",0,0));
 CHECK(capability("{\"textDocument\":{\"signatureHelp\":{\"signatureInformation\":{\"activeParameterSupport\":true,\"parameterInformation\":{\"labelOffsetSupport\":true}}}}}",1,3));
 CHECK(capability("{\"textDocument\":{\"signatureHelp\":{\"signatureInformation\":{\"activeParameterSupport\":0}}}}",0,0));
 CHECK(capability("{\"textDocument\":{\"signatureHelp\":{\"contextSupport\":true,\"signatureInformation\":{\"documentationFormat\":[\"plaintext\",\"markdown\"]}}}}",1,0));
 char* path=xrtPathAbs("signature-wire.xl");CHECK(path);
 const char* source="func make(string value) as func(int) as int\n return func(int number) as int\n return number\n end\nend\nfunc main(...) as int\n return make(\"😀\")(7)\nend\n";
 const char* call=strstr(source,"make(\"😀\")(7)");CHECK(call);size_t offset=(size_t)(call-source)+strlen("make(\"😀\")(");
 xls_semantic_input input={path,source,strlen(source)};xls_signature_help* result=xlsSignatureCreateInContext(NULL,path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && xlsSignatureInfo(result).available);
 xvalue* wire=xlsSignatureHelpValue(result,3);CHECK(wire);const xvalue* sig=xrtValueArrayGet(get(wire,"signatures"),0);const xvalue* param=xrtValueArrayGet(get(sig,"parameters"),0);const xvalue* label=get(param,"label");CHECK(label && xrtValueType(label)==XVALUE_ARRAY && xrtValueCount(label)==2);CHECK(integer(xrtValueArrayGet(label,0),11) && integer(xrtValueArrayGet(label,1),14));CHECK(integer(get(sig,"activeParameter"),0) && integer(get(wire,"activeParameter"),0));
 xvalue* ctx=xrtValueObject();CHECK(ctx && xrtValueObjectSetNew(ctx,sv("triggerKind"),xrtValueInt(3)) && xrtValueObjectSetNew(ctx,sv("isRetrigger"),xrtValueBool(true)) && xrtValueObjectSet(ctx,sv("activeSignatureHelp"),wire));CHECK(xlsSignatureHelpContextValid(ctx));xrtValueRelease(ctx);xrtValueRelease(wire);
 wire=xlsSignatureHelpValue(result,0);CHECK(wire);sig=xrtValueArrayGet(get(wire,"signatures"),0);param=xrtValueArrayGet(get(sig,"parameters"),0);xstrview text={0};CHECK(xrtValueGetString(get(param,"label"),&text) && text.Size==3 && !memcmp(text.Data,"int",3));CHECK(!get(sig,"activeParameter"));CHECK(integer(get(wire,"activeParameter"),0));xrtValueRelease(wire);
 size_t size=0;char* proof=xlsSignatureInputJson(result,&size);CHECK(proof && size);wire=xrtJsonParse((xstrview){proof,size});CHECK(wire && get(wire,"sourceReport") && get(wire,"resolutionReport") && !get(wire,"completion") && !get(wire,"signatures"));xrtValueRelease(wire);xlsSemanticStringFree(proof);
 CHECK(!xlsSignatureHelpValue(result,4));xlsSignatureRelease(result);
 result=xlsSignatureCreateInContext(NULL,path,path,0,&input,1,NULL,NULL,NULL,0);CHECK(result && !xlsSignatureInfo(result).available);wire=xlsSignatureHelpValue(result,3);CHECK(wire && xrtValueType(wire)==XVALUE_NULL);xlsSignatureRelease(result);CHECK(xrtValueType(wire)==XVALUE_NULL);xrtValueRelease(wire);xrtFree(path);return 1;
}
int main(void){if(!run())return 1;printf("signature_help_test passed: %u checks\n",checks);return 0;}
