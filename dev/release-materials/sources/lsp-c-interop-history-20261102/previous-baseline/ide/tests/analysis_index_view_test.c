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
typedef struct expected_token { const char* marker;size_t skip,bytes; } expected_token;
static int highlights(xide_semantic_definitions* d,const char* path,const char* text,
    expected_token cursor,const expected_token* expected,size_t count)
{
    const char* at=strstr(text,cursor.marker);int line,character;CHECK(at&&xideLspOffsetToPosition(text,(int)(at-text+cursor.skip),&line,&character));
    xvalue* out=xideSemanticDocumentHighlights(d,path,line,character);CHECK(out&&xrtValueType(out)==XVALUE_ARRAY&&xrtValueCount(out)==count);
    for(size_t i=0;i<count;++i) {
        const char* token=strstr(text,expected[i].marker);int sl,sc,el,ec;CHECK(token);
        CHECK(xideLspOffsetToPosition(text,(int)(token-text+expected[i].skip),&sl,&sc)&&xideLspOffsetToPosition(text,(int)(token-text+expected[i].skip+expected[i].bytes),&el,&ec));
        const xvalue *row=xrtValueArrayGet(out,i),*range=get(row,"range"),*start=get(range,"start"),*end=get(range,"end");int64 a,b,c,e,kind;
        CHECK(xrtValueGetInt(get(start,"line"),&a)&&xrtValueGetInt(get(start,"character"),&b)&&xrtValueGetInt(get(end,"line"),&c)&&xrtValueGetInt(get(end,"character"),&e)&&xrtValueGetInt(get(row,"kind"),&kind));
        CHECK(a==sl&&b==sc&&c==el&&e==ec&&kind==1);
    }
    xrtValueRelease(out);return 1;
}
typedef struct expected_reference {const char* path;const char* text;expected_token token;int declaration;} expected_reference;
static int references(xide_semantic_definitions* d,const char* path,const char* text,
 expected_token cursor,int includeDeclaration,const expected_reference* expected,size_t count)
{
 const char* query=strstr(text,cursor.marker);int line,character;CHECK(query && count<=16 && xideLspOffsetToPosition(text,(int)(query-text+cursor.skip),&line,&character));
 xide_semantic_reference_view view={0};CHECK(xideSemanticReferences(d,path,line,character,includeDeclaration,&view) && view.count==count);
 unsigned seen=0;
 for(size_t i=0;i<count;++i) {
  xide_semantic_reference item={0};CHECK(xideSemanticReferenceAt(&view,i,&item));size_t matched=count;
  for(size_t j=0;j<count;++j) {
   const expected_reference* e=expected+j;const char* at=strstr(e->text,e->token.marker);CHECK(at);
   int bom=!memcmp(e->text,"\xef\xbb\xbf",3),offset=(int)(at-e->text+e->token.skip);int sl,sc,el,ec;
   CHECK(xideLspOffsetToPosition(e->text,offset,&sl,&sc) && xideLspOffsetToPosition(e->text,offset+(int)e->token.bytes,&el,&ec));
   if(xidePathLexicalEqual(item.path,e->path) && item.start.offset==offset-(bom ? 3 : 0) && item.end.offset==offset+(int)e->token.bytes-(bom ? 3 : 0) &&
      item.start.line==sl && item.start.character==sc && item.end.line==el && item.end.character==ec && item.declaration==e->declaration) {matched=j;break;}
  }
  CHECK(matched<count && !(seen&(1u<<matched)));seen|=1u<<matched;
 }
 xide_semantic_reference out={0};CHECK(!xideSemanticReferenceAt(&view,count,&out));return 1;
}
static int highlightIdentities(void)
{
    char* path=xrtPathAbs("highlight-入口.xl");CHECK(path);
    const char* text="\xef\xbb\xbf" "// 值 choose other\r\ntypedef Count = int\r\nfunc choose(int value) as int\r\n return value\r\nend\r\nfunc choose(string value) as int\r\n return 2\r\nend\r\nfunc main(...) as int\r\n Count 值 = 1; string 文本 = \"😀\"\r\n int other = choose(值)\r\n if true\r\n  int 值 = 2\r\n  other = other + 值\r\n end\r\n return choose(值) + choose(文本) + other\r\nend\r\n";
    xls_semantic_input input={path,text,strlen(text)};char error[256];xls_semantic_check* checked=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    CHECK(checked&&xlsSemanticCheckInfo(checked).accepted);
    xide_semantic_source source={path,text+3,1,1};xide_model_error e={0};xide_semantic_definitions* d=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(checked),&source,1,&e);CHECK(d);xlsSemanticCheckDestroy(checked);
    expected_token outer[]={{"Count 值",6,3},{"other = choose(值)",15,3},{"return choose(值)",14,3}};
    expected_token inner[]={{"int 值 = 2",4,3},{"other + 值",8,3}};
    expected_token other[]={{"int other",4,5},{"other = other",0,5},{"other = other",8,5},{"+ other",2,5}};
    expected_token intCalls[]={{"func choose(int",5,6},{"other = choose(值)",8,6},{"return choose(值)",7,6}};
    expected_token stringCalls[]={{"func choose(string",5,6},{"+ choose(文本)",2,6}};
    expected_token aliases[]={{"typedef Count",8,5},{"Count 值",0,5}};
    CHECK(highlights(d,path,text,outer[1],outer,3)&&highlights(d,path,text,outer[0],outer,3));
    CHECK(highlights(d,path,text,inner[1],inner,2)&&highlights(d,path,text,other[2],other,4));
    CHECK(highlights(d,path,text,intCalls[1],intCalls,3)&&highlights(d,path,text,stringCalls[1],stringCalls,2));
    CHECK(highlights(d,path,text,aliases[1],aliases,2));
    expected_reference callReferences[]={{path,text,intCalls[0],1},{path,text,intCalls[1],0},{path,text,intCalls[2],0}};
    expected_reference shadowReferences[]={{path,text,inner[0],1},{path,text,inner[1],0}};
    expected_reference aliasReferences[]={{path,text,aliases[0],1},{path,text,aliases[1],0}};
    CHECK(references(d,path,text,intCalls[1],1,callReferences,3) && references(d,path,text,intCalls[0],0,callReferences+1,2));
    CHECK(references(d,path,text,inner[1],1,shadowReferences,2) && references(d,path,text,aliases[1],1,aliasReferences,2));
    CHECK(references(d,path,text,(expected_token){"// 值",3,3},1,NULL,0) && references(d,path,text,(expected_token){"😀",0,4},1,NULL,0));
    CHECK(highlights(d,path,text,(expected_token){"// 值",3,3},NULL,0)&&highlights(d,path,text,(expected_token){"😀",0,4},NULL,0));
    CHECK(!xideSemanticDocumentHighlights(d,"unknown.xl",0,0)&&!xideSemanticDocumentHighlights(d,path,-1,0));
    xideSemanticDefinitionsDestroy(d);xrtFree(path);return 1;
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
    expected_reference importedReferences[]={{path,text,{"typed_view_dep.value()",sizeof("typed_view_dep.")-1,5},0},{dep,dependency,{"export func value()",12,5},1}};
    CHECK(references(native,path,text,importedReferences[0].token,1,importedReferences,2) && references(legacy,path,text,importedReferences[0].token,1,importedReferences,2));
    CHECK(references(native,dep,dependency,importedReferences[1].token,0,importedReferences,1) && references(legacy,dep,dependency,importedReferences[1].token,0,importedReferences,1));
    expected_reference genericReferences[]={{path,text,{"keep<T>",5,1},1},{path,text,{"keep<T>(T value)",8,1},0},{path,text,{"value) as T",10,1},0}};
    CHECK(references(native,path,text,genericReferences[1].token,1,genericReferences,3) && references(legacy,path,text,genericReferences[1].token,1,genericReferences,3));
    /* Both navigation tables must outlive the check, AST, collector and DOM.
     * Compare all positions, including whitespace, end boundaries and the
     * second UTF-16 unit of a non-BMP scalar; not just a few chosen tokens. */
    unsigned definitions=0,hovers=0,symbols=0,highlightArrays=0;
    for(size_t i=0;i<count;++i) {
        CHECK(compare(xideSemanticDocumentSymbols(legacy,sources[i].path),xideSemanticDocumentSymbols(native,sources[i].path),&symbols));
        int line=0,character=sources[i].utf16Prefix; size_t size=strlen(sources[i].text),at=0;
        while(at<=size) {
            CHECK(compare(xideSemanticDefinition(legacy,sources[i].path,line,character),xideSemanticDefinition(native,sources[i].path,line,character),&definitions));
            CHECK(compare(xideSemanticHover(legacy,sources[i].path,line,character),xideSemanticHover(native,sources[i].path,line,character),&hovers));
            CHECK(compare(xideSemanticDocumentHighlights(legacy,sources[i].path,line,character),xideSemanticDocumentHighlights(native,sources[i].path,line,character),&highlightArrays));
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
    if(!run() || !highlightIdentities()) return 1;
    printf("analysis_index_view_test passed: %u checks; typed/JSON navigation equivalence, exact source rejection and independent ownership.\n",checks);
    return 0;
}
