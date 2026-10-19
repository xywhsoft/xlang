#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned narrowed,reads,writes,wraps,mutations,emissions;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_UNION_FLOW_H",NULL):
            xl6IrCgenEmitC(&output,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(text&&size==0);
        xrtFree(text);xl6OutputDiscard(&output);++emissions;
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1)) {
        for(size_t i=0;i<unit.parse.diags.count;++i)fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
        return 1;
    }
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    for(size_t i=0;i<module->nodeCount;++i) {
        XL6_IrNode* member=&module->nodes[i];
        if(member->memberAbi!=XL6_IR_MEMBER_ABI_UNION_FIELD &&
           member->memberAbi!=XL6_IR_MEMBER_ABI_UNION_FIELD_DIRECT_STORAGE_WRITE)continue;
        assert(member->unionField<module->unionFieldCount&&member->childCount==1);
        XL6_IrNode* receiver=&module->nodes[module->children[member->firstChild]-1];
        if(receiver->storageType==receiver->type)continue;
        XL6_IrTypeId owner=module->unionFields[member->unionField].ownerType;
        assert(receiver->kind==XL6_IR_NODE_NAME&&receiver->type==owner&&
            xl6IrNullableUnionElementType(module,receiver->storageType)==owner&&
            xl6IrUnionReceiverType(module,receiver)==owner);
        ++narrowed;XL6_IrNode saved=*receiver;
        receiver->type=receiver->storageType;assert(!xl6IrUnionReceiverType(module,receiver));refused(module);*receiver=saved;
        receiver->kind=XL6_IR_NODE_CALL;assert(!xl6IrUnionReceiverType(module,receiver));refused(module);*receiver=saved;
        XL6_IrTypeId other=0;
        for(size_t n=0;n<module->unionLayoutCount;++n)
            if(module->unionLayouts[n].type!=owner)other=module->unionLayouts[n].type;
        assert(other);receiver->type=other;assert(!xl6IrUnionReceiverType(module,receiver));refused(module);*receiver=saved;
        XL6_IrType* wrapper=&module->types.items[receiver->storageType-1];
        XL6_IrTypeId base=wrapper->params[0];wrapper->params[0]=other;
        assert(!xl6IrUnionReceiverType(module,receiver));refused(module);wrapper->params[0]=base;
        assert(xl6IrVerifyModule(module,NULL));
    }
    assert(narrowed>=8);
    if(!xl6IrLowerModule(module,&unit.parse.diags)) {
        for(size_t i=0;i<unit.parse.diags.count;++i)fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
        return 1;
    }
    assert(xl6IrVerifyModule(module,NULL));
    for(size_t i=0;i<module->instructionCount;++i) {
        XL6_IrInstruction* instruction=&module->instructions[i];
        if(instruction->kind==XL6_IR_INSTR_NULLABLE_WRAP&&xl6IrNullableUnionElementType(module,instruction->type))++wraps;
        if(instruction->kind!=XL6_IR_INSTR_UNION_GET&&instruction->kind!=XL6_IR_INSTR_UNION_SET)continue;
        if(instruction->kind==XL6_IR_INSTR_UNION_GET)++reads;else ++writes;
        size_t saved=instruction->elementIndex;assert(saved<module->unionFieldCount);
        XL6_IrTypeId owner=module->unionFields[saved].ownerType;size_t wrong=SIZE_MAX;
        for(size_t n=0;n<module->unionFieldCount;++n)
            if(module->unionFields[n].ownerType!=owner&&module->unionFields[n].type==module->unionFields[saved].type)wrong=n;
        assert(wrong!=SIZE_MAX);instruction->elementIndex=wrong;refused(module);instruction->elementIndex=saved;
        instruction->elementIndex=SIZE_MAX;refused(module);instruction->elementIndex=saved;
    }
    assert(reads>=8&&writes>=5&&wraps>=5&&emissions==mutations*2&&xl6IrVerifyModule(module,NULL));
    printf("nullable union flow: narrowed=%u reads=%u writes=%u wraps=%u mutations=%u zero-output=%u\n",
        narrowed,reads,writes,wraps,mutations,emissions);
    xl6CompileUnitFree(&unit);return 0;
}
