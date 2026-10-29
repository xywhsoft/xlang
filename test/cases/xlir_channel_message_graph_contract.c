#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned owners,nested,nominal,mutations,emissions;
static void refused(XL6_IrModule* module,XL6_IrTypeId owner,bool planFailure)
{
    if (planFailure) {
        XL6_IrChannelValuePlan plan,saved;memset(&plan,0xA5,sizeof(plan));memcpy(&saved,&plan,sizeof(plan));
        XL6_IrTypeId schema=123;XL6_IrNominalIdentitySetId set=456;
        assert(!xl6IrChannelValuePlanBuild(module,owner,&plan) && !memcmp(&plan,&saved,sizeof(plan)));
        assert(!xl6IrChannelConstructionRecipe(module,owner,&schema,&set) && schema==123 && set==456);
    }
    assert(!xl6IrVerifyModule(module,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output output;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_MESSAGE_GRAPH_H",NULL) : xl6IrCgenEmitC(&output,module,NULL)));
        char* body=xl6OutputTakeMemoryText(&output,&size);assert(body && !size);
        xrtFree(body);xl6OutputDiscard(&output);++emissions;
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5);
    for (int file=1;file<argc;++file) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        if (!xl6CompileUnitLoadFile(&unit,argv[file],1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
            for(size_t i=0;i<unit.parse.diags.count;++i)fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
            return 1;
        }
        XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
        for (size_t i=0;i<module->types.count;++i) {
            XL6_IrType* owner=&module->types.items[i];XL6_IrTypeId id=(XL6_IrTypeId)(i+1);
            XL6_IrChannelValuePlan plan={0};
            if (owner->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_CHANNEL || !xl6IrChannelValuePlanBuild(module,id,&plan)) continue;
            ++owners;XL6_IrType* schema=&module->types.items[plan.schemaType-1];
            XL6_IrTypeId saved=owner->messageSchemaType;owner->messageSchemaType=0;
            refused(module,id,true);owner->messageSchemaType=saved;
            XL6_IrSequenceCodecAbi codec=schema->sequenceLayout.codecAbi;schema->sequenceLayout.codecAbi=0;
            refused(module,id,true);schema->sequenceLayout.codecAbi=codec;
            XL6_IrNativeSlotAbi slot=schema->sequenceLayout.slotAbi;schema->sequenceLayout.slotAbi=XL6_IR_NATIVE_SLOT_NONE;
            refused(module,id,true);schema->sequenceLayout.slotAbi=slot;
            saved=owner->params[0];owner->params[0]=id;
            refused(module,id,true);owner->params[0]=saved;
            XL6_IrType* child=&module->types.items[plan.baseElementType-1];
            if (child->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_CHANNEL) {
                ++nested;saved=child->messageSchemaType;child->messageSchemaType=plan.schemaType;
                refused(module,id,true);child->messageSchemaType=saved;
            }
            if (plan.admissionIdentitySet) {
                ++nominal;XL6_IrNominalIdentitySet* set=&module->nominalIdentitySets[plan.admissionIdentitySet-1];
                size_t count=set->identityCount;set->identityCount=SIZE_MAX;refused(module,id,false);set->identityCount=count;
            }
            assert(xl6IrVerifyModule(module,NULL));
        }
        xl6CompileUnitFree(&unit);
    }
    assert(owners>=12 && nested>=4 && nominal>=4 && mutations==owners*4+nested+nominal && emissions==mutations*2);
    printf("Channel graph proof: owners=%u nested=%u nominal=%u mutations=%u zero-output=%u\n",owners,nested,nominal,mutations,emissions);
    return 0;
}
