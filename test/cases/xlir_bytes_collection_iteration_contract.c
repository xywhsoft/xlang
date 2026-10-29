#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"REJECTED_BYTES_CURSOR",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1))goto failed;
    XL6_IrModule* ir=&unit.ir;unsigned mask=0,nullableMask=0,loops=0;
    assert(xl6IrVerifyModule(ir,NULL));
    for(size_t i=0;i<ir->nodeCount;++i){
        XL6_IrNode* node=&ir->nodes[i];
        if(node->sourceKind!=XL6_AST_FOR ||
            (node->loopItemAbi!=XL6_IR_LOOP_ITEM_BYTES_COPY&&node->loopItemAbi!=XL6_IR_LOOP_ITEM_NULLABLE_BYTES_COPY))continue;
        assert(node->childCount==3&&node->loopItemLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE);
        const XL6_IrNode* source=xl6IrNodeGet(ir,ir->children[node->firstChild+1]);
        const XL6_IrType* type=source?xl6IrTypeGet(&ir->types,source->storageType):NULL;assert(type);
        unsigned bit=type->collectionAbi==XL6_IR_COLLECTION_ARRAY?1:type->collectionAbi==XL6_IR_COLLECTION_LIST?2:
            type->collectionAbi==XL6_IR_COLLECTION_SET?4:type->collectionAbi==XL6_IR_COLLECTION_DICT?8:0;
        assert(bit);mask|=bit;if(node->loopItemAbi==XL6_IR_LOOP_ITEM_NULLABLE_BYTES_COPY)nullableMask|=bit;++loops;
        XL6_IrNode saved=*node;
#define BAD(Field,Value) do{node->Field=(Value);refused(ir);*node=saved;}while(0)
        BAD(loopItemAbi,XL6_IR_LOOP_ITEM_STRING_COPY);
        BAD(loopItemAbi,saved.loopItemAbi==XL6_IR_LOOP_ITEM_BYTES_COPY?XL6_IR_LOOP_ITEM_NULLABLE_BYTES_COPY:XL6_IR_LOOP_ITEM_BYTES_COPY);
        BAD(loopItemLifetimeAbi,XL6_IR_LOOP_TARGET_LIFETIME_NONE);
        BAD(loopResourceAbi,XL6_IR_LOOP_RESOURCE_NONE);
        BAD(loopResourceType,XL6_IR_TYPE_INVALID);
#undef BAD
    }
    assert(mask==15&&nullableMask==15&&loops>=8);
    if(!xl6IrLowerModule(ir,&unit.parse.diags))goto failed;
    assert(xl6IrVerifyModule(ir,NULL));
    unsigned borrowed=0;
    for(size_t i=0;i<ir->instructionCount;++i){
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind==XL6_IR_INSTR_RUNTIME_CALL&&ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW)++borrowed;
    }
    assert(borrowed>=loops);
    printf("bytes cursor: four collections, four nullable collections, %u loops, %u borrowed observations, %u rejected C/header mutations\n",loops,borrowed,mutations);
    xl6CompileUnitFree(&unit);return 0;
failed:
    for(size_t i=0;i<unit.parse.diags.count;++i)fprintf(stderr,"%d:%d %s\n",unit.parse.diags.items[i].line,unit.parse.diags.items[i].column,unit.parse.diags.items[i].message);
    xl6CompileUnitFree(&unit);return 2;
}
