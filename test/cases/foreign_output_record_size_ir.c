#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* name)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"unexpected valid output-record IR: %s\n",name);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_RECORD_OUTPUT",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    size_t rc=m->foreignOutputRecordCount,fc=m->foreignOutputFieldCount;
    size_t rp=m->foreignOutputRecordCapacity,fp=m->foreignOutputFieldCapacity;
    XL6_ForeignOutputRecordContract* records=m->foreignOutputRecords;
    XL6_ForeignOutputFieldContract* fields=m->foreignOutputFields;
    assert(rc==7 && fc==24);
    size_t sc=m->foreignOutputSizeCount,sp=m->foreignOutputSizeCapacity;
    XL6_ForeignOutputSizeContract* sizes=m->foreignOutputSizes;assert(sc==rc);
    m->foreignOutputSizes=NULL;refused(m,"lost size rows");m->foreignOutputSizes=sizes;
    m->foreignOutputSizeCount=0;refused(m,"missing size rows");m->foreignOutputSizeCount=sc;
    m->foreignOutputSizeCount=sp+1;refused(m,"invalid size count");m->foreignOutputSizeCount=sc;
    for(size_t i=0;i<sc;++i){
        XL6_ForeignOutputSizeContract saved=sizes[i];
        sizes[i].declaration=0;refused(m,"size declaration");sizes[i]=saved;
        sizes[i].physicalIndex=63;refused(m,"size physical slot");sizes[i]=saved;
        sizes[i].ordinal=1;refused(m,"size logical ordinal cannot masquerade as field ordinal");sizes[i]=saved;
        sizes[i].recordType=1;refused(m,"raw group cannot masquerade as logical record");sizes[i]=saved;
        sizes[i].fieldType=1;refused(m,"raw size type identity");sizes[i]=saved;
        sizes[i].fieldDeclaration=1;refused(m,"raw size field identity");sizes[i]=saved;
        strcpy(sizes[i].field,"iSize.bad");refused(m,"size expression");sizes[i]=saved;
        memset(sizes[i].field,'x',sizeof(sizes[i].field));refused(m,"unterminated size field");sizes[i]=saved;
        XL6_IrNode* n=&m->nodes[saved.declaration-1];uint8 mode=n->foreignCall.valueOutput;
        n->foreignCall.valueOutput=(uint8)(mode&~XL6_FOREIGN_VALUE_OUTPUT_SIZE_INITIALIZED);refused(m,"lost size presence");n->foreignCall.valueOutput=mode;
        if(i+1<sc){XL6_ForeignOutputSizeContract other=sizes[i+1];sizes[i+1]=saved;refused(m,"duplicate physical size");sizes[i+1]=other;}
    }
    uint32 aliased=0;
    for(size_t t=0;t<m->callTargetCount;++t){const XL6_IrCallTarget* target=&m->callTargets[t];
        const XL6_IrNode* node=xl6IrNodeGet(m,target->declaration);
        if(node && node->sourceKind==XL6_AST_EXTERN_FUNC && target->cName && !strcmp(target->cName,"output_record_read")){aliased=target->declaration;break;}
    }
    assert(aliased);
    m->foreignOutputRecordCount=rp+1;refused(m,"record count");m->foreignOutputRecordCount=rc;
    m->foreignOutputRecordCapacity=rc-1;refused(m,"record capacity");m->foreignOutputRecordCapacity=rp;
    m->foreignOutputRecords=NULL;refused(m,"lost records");m->foreignOutputRecords=records;
    m->foreignOutputRecordCount=0;refused(m,"missing record presence");m->foreignOutputRecordCount=rc;
    m->foreignOutputFieldCount=fp+1;refused(m,"field count");m->foreignOutputFieldCount=fc;
    m->foreignOutputFieldCapacity=fc-1;refused(m,"field capacity");m->foreignOutputFieldCapacity=fp;
    m->foreignOutputFields=NULL;refused(m,"lost fields");m->foreignOutputFields=fields;
    m->foreignOutputFieldCount=0;refused(m,"missing fields");m->foreignOutputFieldCount=fc;
    for(size_t i=0;i<rc;++i){
        XL6_ForeignOutputRecordContract saved=records[i];XL6_IrNode* node=&m->nodes[saved.declaration-1];
        records[i].declaration=0;refused(m,"record declaration");records[i]=saved;
        records[i].physicalIndex=63;refused(m,"record physical");records[i]=saved;
        records[i].recordCName[0]=0;refused(m,"empty native record");records[i]=saved;
        strcpy(records[i].recordCName,"Type);bad(");refused(m,"record expression");records[i]=saved;
        memset(records[i].recordCName,'x',sizeof(records[i].recordCName));refused(m,"unterminated record");records[i]=saved;
        uint8 mode=node->foreignCall.valueOutput;
        node->foreignCall.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_STATUS;refused(m,"lost record kind");node->foreignCall.valueOutput=mode;
        node->foreignCall.valueOutput=(uint8)(mode|0x20);refused(m,"unknown mode bits");node->foreignCall.valueOutput=mode;
        uint64 flags=node->flags;node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(m,"missing checked producer contract");node->flags=flags;
        if(i+1<rc){XL6_ForeignOutputRecordContract next=records[i+1];records[i+1]=saved;refused(m,"duplicate record");records[i+1]=next;}
    }
    for(size_t i=0;i<fc;++i){
        XL6_ForeignOutputFieldContract saved=fields[i];
        fields[i].declaration=0;refused(m,"field declaration");fields[i]=saved;
        fields[i].ordinal=63;refused(m,"field ordinal");fields[i]=saved;
        fields[i].logicalType=0;refused(m,"field type");fields[i]=saved;
        fields[i].field[0]=0;refused(m,"empty field");fields[i]=saved;
        strcpy(fields[i].field,"_private");refused(m,"private field");fields[i]=saved;
        strcpy(fields[i].field,"first.value");refused(m,"nested expression");fields[i]=saved;
        memset(fields[i].field,'x',sizeof(fields[i].field));refused(m,"unterminated field");fields[i]=saved;
        if(i+1<fc){XL6_ForeignOutputFieldContract next=fields[i+1];fields[i+1]=saved;refused(m,"duplicate field");fields[i+1]=next;}
    }
    for(size_t i=0;i<m->foreignOutputViewCount;++i){
        XL6_ForeignOutputViewContract saved=m->foreignOutputViews[i];XL6_ForeignOutputViewContract* row=&m->foreignOutputViews[i];
        assert(row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE);
        row->physicalIndex=63;refused(m,"retain physical");*row=saved;
        row->ordinal=63;refused(m,"retain ordinal");*row=saved;
        row->logicalType=0;refused(m,"retain result type");*row=saved;
        row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_TEXT;refused(m,"text field unsupported");*row=saved;
        row->retainSymbol=0;refused(m,"missing retain");*row=saved;
        row->rollbackSymbol=0;refused(m,"missing rollback");*row=saved;
        row->retainSymbol=saved.rollbackSymbol;refused(m,"retain signature");*row=saved;
        row->retainSuccess=0;refused(m,"retain status");*row=saved;
        if(row->declaration==aliased){row->retainSuccess=XL6_FOREIGN_RETAIN_POSITIVE;refused(m,"same-C retained field rule");*row=saved;}
    }
    for(size_t t=0;t<m->callTargetCount;++t){
        XL6_IrCallTarget* target=&m->callTargets[t];
        const XL6_IrNode* node=xl6IrNodeGet(m,target->declaration);
        if(!node || !xl6ForeignValueOutputIsRecord(node->foreignCall.valueOutput))continue;
        XL6_IrTypeId type=target->returnType;target->returnType=0;refused(m,"target tuple type");target->returnType=type;
        int32 code=target->nativeSuccessCode;target->nativeSuccessCode=31;refused(m,"target status code");target->nativeSuccessCode=code;
        XL6_IrCallOutputResultAbi output=target->outputResultAbi;target->outputResultAbi=XL6_IR_CALL_OUTPUT_RESULT_NONE;refused(m,"target physical status");target->outputResultAbi=output;
    }
    /* A valid field selector can still disagree with the second declaration
     * of the same C target. This is metadata equality, not C name guessing. */
    unsigned checkedSelector=0;
    for(size_t i=0;i<fc;++i){
        if(fields[i].declaration==aliased && !strcmp(fields[i].field,"code")){
            XL6_ForeignOutputFieldContract saved=fields[i];strcpy(fields[i].field,"otherCode");refused(m,"same-C field selector");fields[i]=saved;++checkedSelector;break;
        }
    }
    assert(checkedSelector==1 && xl6IrVerifyModule(m,NULL));printf("borrowed output record: %lu groups/%lu fields, %u mutations rejected with zero C/header bytes\n",(unsigned long)rc,(unsigned long)fc,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
