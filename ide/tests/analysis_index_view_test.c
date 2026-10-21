#include "analysis/check.h"
#include "services/semantic_definition.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_index_view_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static const xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,xrtStrView(key)); }
static const char* string(const xvalue* v) { xstrview s; return xrtValueGetString(v,&s) ? s.Data : NULL; }
static int compare(xvalue* a,xvalue* b,unsigned* found)
{
    int okay=a && b && xrtValueEqual(a,b);
    if(a && xrtValueType(a)!=XVALUE_NULL) ++*found;
    xrtValueRelease(a); xrtValueRelease(b); return okay;
}
static int run(void)
{
    char *path=xrtPathAbs("typed-view-memory.xl"),*dep=xrtPathAbs("typed_view_dep.xl"); CHECK(path && dep);
    const char* text="\xef\xbb\xbf" "import typed_view_dep\r\nprotocol Named\r\n func name() as string\r\nend\r\nclass Base\r\n int value\r\nend\r\nclass Child extends Base\r\nend\r\nfunc keep<T>(T value) as T\r\n return value\r\nend\r\nfunc bounded<T is Named>(T value) as int\r\n return 0\r\nend\r\nfunc main(...) as int\r\n string 结果 = \"😀\"\r\n auto child = Child()\r\n return keep::<int>(child.value + typed_view_dep.value())\r\nend\r\n";
    const char* dependency="export func value() as int\n return 7\nend\n";
    xls_semantic_input inputs[2]={{path,text,strlen(text)},{dep,dependency,strlen(dependency)}}; char error[256];
    xls_semantic_check* checked=xlsSemanticCheckCreate(path,inputs,2,1,NULL,NULL,error,sizeof(error));
    if(!checked) fprintf(stderr,"typed view check: %s\n",error);
    CHECK(checked && xlsSemanticCheckInfo(checked).accepted);
    char* json=xlsSemanticCheckJson(checked,NULL); CHECK(json);
    xvalue* raw=xrtJsonParse(xrtStrView(json)); CHECK(raw);
    const xvalue* files=get(get(raw,"sourceReport"),"files");
    size_t count=xrtValueCount(files); CHECK(count==2);
    xide_semantic_source sources[2];
    for(size_t i=0;i<count;++i) {
        const xvalue* file=xrtValueArrayGet(files,i); const char* name=string(get(file,"path")); int64 kind;
        CHECK(name && xrtValueGetInt(get(file,"kind"),&kind));
        size_t at=xidePathLexicalEqual(name,path) ? 0 : xidePathLexicalEqual(name,dep) ? 1 : 2; CHECK(at<2);
        int bom=inputs[at].size>=3 && !memcmp(inputs[at].text,"\xef\xbb\xbf",3);
        sources[i]=(xide_semantic_source){inputs[at].path,inputs[at].text+(bom ? 3 : 0),(int)kind,bom};
    }
    xide_model_error e={0};
    xide_semantic_definitions* legacy=xideSemanticDefinitionsCreate(get(raw,"semanticIndex"),sources,count,&e);
    xide_semantic_definitions* native=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(checked),sources,count,&e);
    CHECK(legacy && native);
    /* Incorrect exact-source bytes must fail the SAME endpoint validation. */
    xide_semantic_source wrong[2]={sources[0],sources[1]}; wrong[0].text="";
    CHECK(!xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(checked),wrong,count,&e));
    CHECK(!xideSemanticDefinitionsCreate(get(raw,"semanticIndex"),wrong,count,&e));
    CHECK(!xideSemanticDefinitionsCreateTyped(NULL,sources,count,&e));
    xlsSemanticCheckDestroy(checked); xrtValueRelease(raw); xlsSemanticStringFree(json);
    /* Both navigation tables must outlive the check, AST, collector and DOM.
     * Compare all positions, including whitespace, end boundaries and the
     * second UTF-16 unit of a non-BMP scalar; not just a few chosen tokens. */
    unsigned definitions=0,hovers=0,symbols=0;
    for(size_t i=0;i<count;++i) {
        CHECK(compare(xideSemanticDocumentSymbols(legacy,sources[i].path),xideSemanticDocumentSymbols(native,sources[i].path),&symbols));
        int line=0,character=sources[i].utf16Prefix; size_t size=strlen(sources[i].text),at=0;
        while(at<=size) {
            CHECK(compare(xideSemanticDefinition(legacy,sources[i].path,line,character),xideSemanticDefinition(native,sources[i].path,line,character),&definitions));
            CHECK(compare(xideSemanticHover(legacy,sources[i].path,line,character),xideSemanticHover(native,sources[i].path,line,character),&hovers));
            if(at==size) break;
            unsigned char c=(unsigned char)sources[i].text[at]; size_t n=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
            if(c=='\r' && at+1<size && sources[i].text[at+1]=='\n') n=2;
            if(n==4) {
                CHECK(compare(xideSemanticDefinition(legacy,sources[i].path,line,character+1),xideSemanticDefinition(native,sources[i].path,line,character+1),&definitions));
                CHECK(compare(xideSemanticHover(legacy,sources[i].path,line,character+1),xideSemanticHover(native,sources[i].path,line,character+1),&hovers));
            }
            if(c=='\r' || c=='\n') { ++line; character=0; } else character+=n==4 ? 2 : 1;
            at+=n;
        }
    }
    CHECK(definitions>20 && hovers>20 && symbols==2);
    xideSemanticDefinitionsDestroy(legacy); xideSemanticDefinitionsDestroy(native); xrtFree(path); xrtFree(dep); return 1;
}
int main(void)
{
    if(!run()) return 1;
    printf("analysis_index_view_test passed: %u checks; typed/JSON navigation equivalence, exact source rejection and independent ownership.\n",checks);
    return 0;
}
