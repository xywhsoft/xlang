#include "src/ir/xl6_ir.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; XL6_DiagList diagnostics; size_t size=1;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_VOCABULARY",&diagnostics) :
            xl6IrCgenEmitC(&out,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&out); xl6DiagFree(&diagnostics);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3);
    for(unsigned lane=0;lane<2;++lane) {
        XL6_ParseResult parse; XL6_IrModule module;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&module);
        assert(xl6ParseFile(argv[lane+1],&parse));
        assert(xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"implicit_collections"));
        assert(xl6IrBuildModuleFromAst(&module,parse.root,&parse.diags));
        XL6_IrTypeId any=xl6IrSetAnyType(&module); assert(any);
        XL6_IrValueCollectionRecipe recipe={0};
        assert(xl6IrValueCollectionConstructionRecipe(&module,any,&recipe));
        assert(recipe.family==XL6_IR_COLLECTION_SET && recipe.ownerType==any &&
            recipe.setKeyAbi==XL6_IR_SET_KEY_VALUE_CONTENT_OR_REFERENCE_IDENTITY_V1);
        assert(module.types.items[any-1].typeValueId==module.runtimeTypeMap.setTypeValueId);
        assert(xl6IrLowerModule(&module,&parse.diags) && xl6IrVerifyModule(&module,&parse.diags));
        unsigned erasedSets=0;
        for(size_t i=0;i<module.instructionCount;++i) {
            const XL6_IrInstruction* op=&module.instructions[i];
            if(op->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_NEW && op->type==any) ++erasedSets;
        }
        if(!lane) assert(erasedSets==2);
        else {
            assert(!erasedSets);
            XL6_ArtifactRuntimePlan artifact={0};
            assert(xl6ArtifactRuntimePlanBuild(&module,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&artifact,NULL));
            assert(!xl6ArtifactRuntimeSetHas(&artifact.components,XL6_ARTIFACT_SET_NATIVE));
        }
        XL6_IrType* owner=&module.types.items[any-1];
        XL6_IrTypeId schema=owner->collectionValueSchemaType;
        owner->collectionValueSchemaType=0; refused(&module); owner->collectionValueSchemaType=schema;
        XL6_IrSetKeyAbi key=owner->setKeyAbi;
        owner->setKeyAbi=XL6_IR_SET_KEY_NONE; refused(&module); owner->setKeyAbi=key;
        uint64_t identity=owner->typeValueId;
        owner->typeValueId=0; refused(&module); owner->typeValueId=identity;
        assert(xl6IrVerifyModule(&module,NULL));
        xl6IrModuleFree(&module); xl6ParseResultFree(&parse);
    }
    puts("implicit collection vocabulary: erased native producers, six corruptions refused and unused Set pruned");
    return 0;
}
