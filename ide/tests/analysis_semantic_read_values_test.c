#define main semanticReadValuesBaseMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/frontend_sem_read_values.h"

static int values(void)
{
    char name[]="内置😀",abi[]="nativeABI";
    XL6_TypeInfo original={.id=17,.abiId=0xf123456789abcdefULL,.name=name,.abiName=abi,
        .kind=XL6_TYPE_BUILTIN,.allowGeneric=1,.genericArity=2};
    xls_sem_read_value v={0};CHECK(!xlsSemReadValueEqual(&v,&original));CHECK(xlsSemReadValueInit(&v,&original));
    CHECK(v.ready && v.present && v.stringsBytes==strlen(name)+strlen(abi)+2 && v.type.name!=name && v.type.abiName!=abi && !v.type.decl);
    CHECK(xlsSemReadValueEqual(&v,&original));
    for(unsigned field=0;field<8;++field){XL6_TypeInfo changed=original;
        switch(field){case 0:++changed.id;break;case 1:++changed.abiId;break;case 2:changed.name="other";break;
        case 3:changed.abiName="otherABI";break;case 4:changed.kind=XL6_TYPE_ALIAS;break;
        case 5:changed.allowGeneric=0;break;case 6:changed.genericArity=-1;break;default:changed.decl=(XL6_AstNode*)&changed;break;}
        CHECK(!xlsSemReadValueEqual(&v,&changed));
    }
    name[0]='x';abi[0]='y';CHECK(!xlsSemReadValueEqual(&v,&original));
    original.name="内置😀";original.abiName="nativeABI";CHECK(xlsSemReadValueEqual(&v,&original));
    xlsSemReadValueUnit(&v);CHECK(!v.ready && !v.type.name && !v.type.abiName);xlsSemReadValueUnit(&v);
    original.name=NULL;original.abiName="";CHECK(xlsSemReadValueInit(&v,&original) && xlsSemReadValueEqual(&v,&original));
    XL6_TypeInfo other=original;other.name="";CHECK(!xlsSemReadValueEqual(&v,&other));other=original;other.abiName=NULL;CHECK(!xlsSemReadValueEqual(&v,&other));
    xlsSemReadValueUnit(&v);CHECK(xlsSemReadValueInit(&v,NULL) && v.ready && !v.present && !v.stringsBytes);
    CHECK(xlsSemReadValueEqual(&v,NULL) && !xlsSemReadValueEqual(&v,&original));xlsSemReadValueUnit(&v);
    original.decl=(XL6_AstNode*)&original;CHECK(!xlsSemReadValueInit(&v,&original) && !v.ready && !v.type.decl);
    return 1;
}
static char* valueProgram(unsigned version)
{
    xstrbuf text={0};const char* arg=version>=2 ? "input" : "value";
    int okay=xrtStrBufAppendFormat(&text,"\xef\xbb\xbf// 中文😀\r\nfunc identity(%s %s) as int\r\n return %s\r\nend\r\n",version==5 ? "string" : "int",arg,version==5 ? "0" : arg);
    if(version>=3)okay=okay && xrtStrBufAppendFormat(&text,"func identity(string text,string pad=\"%s\\0😀\") as int\r\n return 0\r\nend\r\n",version>=4 ? "b" : "a");
    for(unsigned i=0;okay && i<32;++i){okay=xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local=identity(value)\r\n",i);
        for(unsigned j=0;okay && j<16;++j)okay=xrtStrBufAppend(&text,xrtStrView(" local=identity(local)+1\r\n"));
        okay=okay && xrtStrBufAppend(&text,xrtStrView(" return local\r\nend\r\n"));}
    okay=okay && xrtStrBufAppendFormat(&text,"func unused(int %s) as int\r\n return %s\r\nend\r\nfunc main(...) as int\r\n return work_0(7)\r\nend\r\n",version ? "other" : "value",version ? "other" : "value");
    char* body=okay ? xrtStrDup(text.Data) : NULL;xrtStrBufFree(&text);return body;
}
static int variants(void)
{
    char* path=xrtPathAbs("semantic-read-values-😀.xl");CHECK(path);xls_sem_read_work reference[7]={0};
    for(unsigned variant=0;variant<6;++variant){xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,0,0};
        xls_sem_value_control outerValue,innerValue;xls_sem_scratch_control outerScratch,innerScratch;
        xlsFrontendSemValuesBegin(&outerValue,!(variant&1));xlsFrontendSemValuesBegin(&innerValue,1);
        size_t budget=variant>=4 ? 1 : variant&2 ? 0 : 8u*1024u*1024u;
        xlsFrontendSemScratchBegin(&outerScratch,budget);xlsFrontendSemScratchBegin(&innerScratch,8u*1024u*1024u);
        for(unsigned version=0;version<7;++version){char* body=valueProgram(version);CHECK(body);xls_semantic_check* result=update(session,&doc,1,body);
            CHECK(result && xlsSemanticCheckInfo(result).accepted==(version!=5) && fresh(result,&doc,1));xls_sem_read_work w=xlsSemanticCheckReads(result);
            if(!version){CHECK(w.observations>10000);CHECK((w.typedComparisons>10000)==!(variant&1));
                CHECK((w.scratchReuses>100)==(budget>1));CHECK((w.scratchBytes>0)==(budget>1) && w.scratchBytes<=budget);
                CHECK((w.valueBytes>0)==!(variant&1) && w.valueBytes<=1024u*1024u);}
            if(version==1)CHECK(w.contextHits && w.reused>=32);
            if(version==2 || version==4)CHECK(w.contextHits && w.rejected>=32);
            if(!variant)reference[version]=w;
            else CHECK(w.bytes==reference[version].bytes && w.recorded==reference[version].recorded && w.negative==reference[version].negative && w.matched==reference[version].matched && w.rejected==reference[version].rejected && w.reused==reference[version].reused);
            printf("variant=%u v=%u observations=%llu typed=%llu scratch=%llu peak=%llu reused=%llu\n",variant,version,(unsigned long long)w.observations,(unsigned long long)w.typedComparisons,(unsigned long long)w.scratchReuses,(unsigned long long)w.scratchBytes,(unsigned long long)w.reused);
            xlsSemanticCheckDestroy(result);xrtFree(body);
        }
        xlsFrontendSemScratchEnd(&innerScratch);xlsFrontendSemScratchEnd(&outerScratch);xlsFrontendSemValuesEnd(&innerValue);xlsFrontendSemValuesEnd(&outerValue);
        xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);
    }
    xrtFree(path);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!values() || !variants())return 1;
    printf("analysis_semantic_read_values_test passed: %u checks;all registry fields/owned UTF8/negative/type-tree refusal,nested typed/serialized and retained/fresh-buffer comparisons,actual binary-default invalidation and complete fresh AST/index/report/IR parity\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
