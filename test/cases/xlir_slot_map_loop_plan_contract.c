#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations,loops,keyOnly,pairs,discarded,acquisitions;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_SLOT_MAP_LOOP",NULL)
            : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);
        assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL));
    for(size_t i=0;i<ir->nodeCount;++i) {
        XL6_IrNode* loop=&ir->nodes[i];
        if(loop->loopAbi!=XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY &&
            loop->loopAbi!=XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY_VALUE) continue;
        ++loops; XL6_IrNode saved=*loop;
        const XL6_IrNode* targets=xl6IrNodeGet(ir,ir->children[loop->firstChild]);
        const XL6_IrNode* source=xl6IrNodeGet(ir,ir->children[loop->firstChild+1]);
        XL6_IrValueCollectionRecipe recipe={0}; XL6_IrTypeId key=0,keyArray=0;
        assert(xl6IrSlotMapLoopRecipe(ir,source->storageType,&recipe,&key,&keyArray));
        XL6_IrLoopSourceAcquisitionAbi acquisition=xl6IrLoopSourceAcquisition(ir,source);
        assert(acquisition>=XL6_IR_LOOP_SOURCE_NAMED_BORROW && acquisition<=XL6_IR_LOOP_SOURCE_OBSERVED_COLLECTION);
        acquisitions|=1u<<acquisition;
        assert(xl6IrSlotMapLoopKeyType(ir,key) && recipe.ownerType==source->storageType);
        assert(recipe.codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT && keyArray!=recipe.ownerType);
        assert(loop->loopResourceAbi==XL6_IR_LOOP_RESOURCE_XVALUE_ITERATOR_DESTROY);
        const XL6_IrType* cursor=xl6IrTypeGet(&ir->types,loop->loopResourceType);
        assert(cursor && cursor->kind==XL6_IR_TYPE_VALUE_ITERATOR && cursor->params[0]==source->storageType);
        assert(loop->loopErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(targets->childCount==1 || targets->childCount==2);
        if(targets->childCount==1) {
            ++keyOnly; assert(loop->loopAbi==XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY);
            assert(loop->loopItemAbi==XL6_IR_LOOP_ITEM_NONE);
            assert(loop->loopItemLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_NONE);
        } else {
            ++pairs; assert(loop->loopAbi==XL6_IR_LOOP_ABI_SLOT_MAP_ITERATOR_KEY_VALUE);
            const XL6_IrNode* value=xl6IrNodeGet(ir,ir->children[targets->firstChild+1]);
            assert(value->storageType==recipe.elementType);
        }
        const XL6_IrNode* target=xl6IrNodeGet(ir,ir->children[targets->firstChild]);
        assert(target->storageType==key);
        if(loop->loopKeyAbi==XL6_IR_LOOP_KEY_DISCARD) {
            ++discarded; assert(!target->symbol && loop->loopKeyLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_NONE);
        } else {
            assert(loop->loopKeyAbi==XL6_IR_LOOP_KEY_NATIVE_VALUE_COPY);
            assert(loop->loopKeyLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE);
            assert(target->constructionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
        }
#define BAD(Field,Value) do { loop->Field=(Value); refused(ir); *loop=saved; } while(0)
        BAD(loopAbi,XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY_VALUE);
        BAD(loopKeyAbi,XL6_IR_LOOP_KEY_INTEGER_I64);
        BAD(loopKeyLifetimeAbi,XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE);
        BAD(loopErrorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(loopResourceAbi,XL6_IR_LOOP_RESOURCE_XVALUE_SHALLOW_SNAPSHOT_RELEASE);
        BAD(loopResourceType,source->storageType);
        BAD(loopItemAbi,XL6_IR_LOOP_ITEM_DYNAMIC_LOGICAL_XVALUE);
        BAD(loopItemLifetimeAbi,saved.loopItemLifetimeAbi==XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE
            ? XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE : XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE);
#undef BAD
        XL6_IrType* owner=&ir->types.items[source->storageType-1]; XL6_IrType ownerSaved=*owner;
        owner->collectionValueSchemaType=0; refused(ir); *owner=ownerSaved;
        owner->runtimeResourceAbi=XL6_IR_RUNTIME_RESOURCE_STACK; refused(ir); *owner=ownerSaved;
        XL6_IrType* keys=&ir->types.items[keyArray-1]; XL6_IrType keysSaved=*keys;
        keys->sequenceLayout.transferAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN; refused(ir); *keys=keysSaved;
        keys->sequenceLayout.codecAbi=XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1; refused(ir); *keys=keysSaved;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(loops==18 && keyOnly==3 && pairs==15 && discarded==3 && acquisitions==14);
    assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));
    xl6CompileUnitFree(&unit);
    printf("slot-map loop plans=%u key-only=%u pairs=%u discard=%u mutations=%u; native backend admitted\n",
        loops,keyOnly,pairs,discarded,mutations);
    return 0;
}
