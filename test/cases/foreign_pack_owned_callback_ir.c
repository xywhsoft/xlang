#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"accepted callback group damage: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_GROUP",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(text && !size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    assert(sizeof(XL6_ForeignCallFacts)==88 && sizeof(XL6_ForeignPackFieldContract)==220);
    unsigned groups=0,fields=0;
    for(size_t i=0;i<ir->foreignPackCount;++i){
        XL6_ForeignPackContract* pack=&ir->foreignPacks[i];if(!pack->callbackOwnerPhysicalIndexPlusOne)continue;
        ++groups;XL6_ForeignPackContract saved=*pack;
        pack->callbackOwnerPhysicalIndexPlusOne=0;refused(ir,"lost group owner");*pack=saved;
        pack->callbackOwnerPhysicalIndexPlusOne=65;refused(ir,"owner outside physical range");*pack=saved;
        pack->callbackOwnerPhysicalIndexPlusOne=pack->physicalIndex+1;refused(ir,"owner overlaps descriptor");*pack=saved;
        pack->callbackOwnerPhysicalIndexPlusOne=64;refused(ir,"owner never reached by physical steps");*pack=saved;
        pack->callbackUserField[0]=0;refused(ir,"lost userdata field");*pack=saved;
        const char* occupied=NULL;
        for(size_t f=0;f<ir->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&ir->foreignPackFields[f];
            if(field->declaration==pack->declaration && field->packPhysicalIndex==pack->physicalIndex &&
                field->kind==XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK){occupied=field->field;break;}
        }
        assert(occupied);strcpy(pack->callbackUserField,occupied);refused(ir,"userdata overlaps callback field");*pack=saved;
        strcpy(pack->callbackUserField,"nested.user");refused(ir,"unwitnessed parent record");*pack=saved;
        memset(pack->callbackUserField,'x',sizeof(pack->callbackUserField));refused(ir,"unterminated userdata path");*pack=saved;
        pack->callbackGroupOwnerCName[0]=0;refused(ir,"missing derived owner ABI");*pack=saved;
        strcpy(pack->callbackGroupOwnerCName,"WrongOwner");refused(ir,"wrong derived owner ABI");*pack=saved;
        memset(pack->callbackGroupOwnerCName,'x',sizeof(pack->callbackGroupOwnerCName));refused(ir,"unterminated owner ABI");*pack=saved;
    }
    for(size_t i=0;i<ir->foreignPackFieldCount;++i){
        XL6_ForeignPackFieldContract* field=&ir->foreignPackFields[i];
        if(field->kind!=XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK)continue;
        ++fields;XL6_ForeignPackFieldContract saved=*field;
        field->kind=XL6_FOREIGN_PACK_FIELD_INVALID;refused(ir,"lost member field");*field=saved;
        field->kind=XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK;refused(ir,"synchronous member substitution");*field=saved;
        field->kind=XL6_FOREIGN_PACK_FIELD_CALLBACK_USER;refused(ir,"individual userdata substitution");*field=saved;
        field->parameter=63;refused(ir,"member outside pack");*field=saved;
        field->logicalType=0;refused(ir,"lost callable type");*field=saved;
        field->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(ir,"spurious span count");*field=saved;
        field->recordParentIndexPlusOne^=1;refused(ir,"wrong parent");*field=saved;
        strcpy(field->physicalCName,"void*");refused(ir,"wrong exact callback ABI");*field=saved;
        memset(field->field,'x',sizeof(field->field));refused(ir,"unterminated member path");*field=saved;
        XL6_IrNode* node=&ir->nodes[field->declaration-1];
        XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(ir,node->symbol);assert(target);
        XL6_ForeignParameterContract* callback=(XL6_ForeignParameterContract*)xl6IrForeignParameterGet(ir,target,field->parameter);assert(callback);
        XL6_ForeignParameterContract original=*callback;
        callback->kind=XL6_FOREIGN_PARAMETER_SYNC_CALLBACK;refused(ir,"lost retained preparation");*callback=original;
        strcpy(callback->callbackOwnerCName,"WrongOwner");refused(ir,"members disagree on owner ABI");*callback=original;
        callback->callbackOwnerIndexPlusOne=65;refused(ir,"unknown context owner");*callback=original;
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[field->parameter];
        target->parameterPhysicalAbis[field->parameter]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(ir,"lost member preparation");
        target->parameterPhysicalAbis[field->parameter]=physical;
    }
    assert(groups>=2 && fields==groups*2 && xl6IrVerifyModule(ir,NULL));
    printf("PACK_OWNED_CALLBACK_IR_OK groups=%u fields=%u mutations=%u facts=88 field=220\n",groups,fields,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
