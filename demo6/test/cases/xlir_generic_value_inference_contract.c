#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char source[] =
    "union First\n int number\nend\nunion Other\n int number\nend\n"
    "func makeMaybe<T>(T value) as T?\n return value\nend\n"
    "async func makeLater<T>(T value) as T?\n return value\nend\n"
    "func otherMaybe(Other? value) as Other?\n return value\nend\n"
    "async func otherLater(Other? value) as Other?\n return value\nend\n"
    "First item\nitem.number=42\nFirst? stored=item\n"
    "makeMaybe(stored)\nmakeLater(stored)\n"
    "Other other\nother.number=7\notherMaybe(other)\notherLater(other)\n";

static XL6_IrNode* call(XL6_IrModule* module, const char* name)
{
    for (size_t i=0; i<module->nodeCount; ++i) {
        XL6_IrNode* node=&module->nodes[i];
        if (node->kind!=XL6_IR_NODE_CALL || !node->childCount) continue;
        const XL6_IrNode* callee=xl6IrNodeGet(module,module->children[node->firstChild]);
        if (callee && callee->kind==XL6_IR_NODE_NAME && callee->text && !strcmp(callee->text,name)) return node;
    }
    assert(!"missing concrete source call"); return NULL;
}

int main(void)
{
    unsigned refused=0, emissions=0;
    for (unsigned mode=0; mode<5; ++mode) {
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadSource(&unit,source,"test/cases/inference_contract.xl",1));
        XL6_IrModule* module=&unit.ir;
        XL6_IrNode* maybe=call(module,"makeMaybe");
        XL6_IrNode* later=call(module,"makeLater");
        const XL6_IrType* nullable=xl6IrTypeGet(&module->types,maybe->storageType);
        assert(nullable && nullable->kind==XL6_IR_TYPE_NULLABLE && nullable->paramCount==1);
        XL6_IrTypeId plain=nullable->params[0];
        assert(xl6IrTypeGet(&module->types,plain)->kind==XL6_IR_TYPE_UNION);
        if (mode==1) maybe->type=maybe->storageType=call(module,"otherMaybe")->storageType;
        if (mode==2) later->type=later->storageType=call(module,"otherLater")->storageType;
        if (mode==3) later->type=later->storageType=maybe->storageType;
        if (mode==4) maybe->type=maybe->storageType=plain;
        if (mode) {
            XL6_IrNode* changed=(mode==2 || mode==3) ? later : maybe;
            for (size_t i=0; i<module->callSiteCount; ++i)
                if (module->callSites[i].origin==(XL6_IrNodeId)(changed-module->nodes+1))
                    module->callSites[i].resultType=changed->storageType;
        }
        if (mode==0) {
            assert(xl6IrLowerModule(module,&unit.parse.diags));
            assert(xl6IrVerifyModule(module,NULL));
        } else {
            assert(!xl6IrLowerModule(module,&unit.parse.diags));
            bool diagnosed=false;
            for (size_t i=0; i<unit.parse.diags.count; ++i)
                if (strstr(unit.parse.diags.items[i].message,"resolved concrete type arguments")) diagnosed=true;
            if (!diagnosed && mode<=2) {
                fprintf(stderr,"unexpected inference rejection, mode=%u\n",mode);
                for (size_t i=0; i<unit.parse.diags.count; ++i) fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
            }
            assert(diagnosed || (mode>=3 && unit.parse.diags.count));
            for (unsigned header=0; header<2; ++header) {
                XL6_Output output; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&output));
                assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_INFERENCE_H",NULL) : xl6IrCgenEmitC(&output,module,NULL)));
                char* body=xl6OutputTakeMemoryText(&output,&size); assert(body && size==0);
                xrtFree(body); xl6OutputDiscard(&output); ++emissions;
            }
            ++refused;
        }
        xl6CompileUnitFree(&unit);
    }
    assert(refused==4 && emissions==8);
    printf("generic value inference: invalid-results=%u zero-output=%u\n",refused,emissions);
    return 0;
}
