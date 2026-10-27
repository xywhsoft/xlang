#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned sourceChecks, mutations;
static const char native[]=
    "class Resource\n private native point handle\n"
    " extern \"constructor_resource_create\" create()\n"
    " extern \"constructor_resource_destroy\" destroy()\nend\n";
static void diagnostics(const XL6_DiagList* diags)
{
    for(size_t i=0;i<diags->count;++i)
        fprintf(stderr,"%d:%d %s\n",diags->items[i].line,diags->items[i].column,diags->items[i].message);
}
static void source(const char* name,const char* body,int expected)
{
    char text[8192];assert(snprintf(text,sizeof(text),"%s%s",native,body)>0);
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    assert(xl6ParseSource(text,name,&parsed));assert(xl6StdlibExpandModule(parsed.root,&parsed.diags));
    int checked=xl6SemCheckModule(parsed.root,&parsed.diags,"constructor_contract");
    if(checked!=expected){fprintf(stderr,"semantic constructor case: %s expected %d got %d\n",name,expected,checked);diagnostics(&parsed.diags);abort();}
    if(!expected){
        int relevant=0;
        for(size_t i=0;i<parsed.diags.count;++i)
            if(strstr(parsed.diags.items[i].message,"constructor")||strstr(parsed.diags.items[i].message,"incomplete self")||
                strstr(parsed.diags.items[i].message,"native resource needs explicit"))relevant=1;
        if(!relevant){fprintf(stderr,"unexpected negative diagnostic: %s\n",name);diagnostics(&parsed.diags);}
        assert(relevant);
    }else{
        XL6_IrModule ir;xl6IrModuleInit(&ir);
        if(!xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags)){fprintf(stderr,"Typed constructor case: %s\n",name);diagnostics(&parsed.diags);assert(0);}
        assert(xl6IrVerifyModule(&ir,NULL));
        if(!xl6IrLowerModule(&ir,&parsed.diags)){fprintf(stderr,"Lowered constructor case: %s\n",name);diagnostics(&parsed.diags);assert(0);}
        assert(xl6IrVerifyModule(&ir,NULL));xl6IrModuleFree(&ir);
    }
    ++sourceChecks;xl6ParseResultFree(&parsed);
}
static void refused(XL6_IrModule* ir)
{
    int verified=xl6IrVerifyModule(ir,NULL);
    if(verified)fprintf(stderr,"constructor mutation %u was accepted\n",mutations);
    assert(!verified);
    if(ir->stage==XL6_IR_STAGE_LOWERED){
        for(unsigned header=0;header<2;++header){
            XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
            assert(!(header?xl6IrCgenEmitHeader(&out,ir,"REJECT_CONSTRUCTOR",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
            char* text=xl6OutputTakeMemoryText(&out,&size);assert(size==0);xrtFree(text);xl6OutputDiscard(&out);
        }
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);
#define OWNER(body) "class Owner\n private Resource value\n create(bool choice)\n" body "\n end\nend\n"
    source("missing.xl",OWNER(""),0);
    source("assigned.xl",OWNER("self.value = Resource()"),1);
    source("rhs_read.xl",OWNER("self.value = self.value"),0);
    source("return.xl",OWNER("if choice\n return\nend\nself.value = Resource()"),0);
    source("throw.xl",OWNER("if choice\n throw \"stop\"\nend\nself.value = Resource()"),1);
    source("if.xl",OWNER("if choice\n self.value = Resource()\nelse\n self.value = Resource()\nend"),1);
    source("zero_loop.xl",OWNER("while choice\n self.value = Resource()\nend"),0);
    source("do_loop.xl",OWNER("do\n self.value = Resource()\nwhile false end"),1);
    source("catch_write.xl",OWNER("try\n self.value = Resource()\ncatch e\n self.value = Resource()\nend"),1);
    source("catch_read.xl",OWNER("try\n self.value = Resource()\ncatch e\n auto bad = self.value\nend"),0);
    source("finally.xl",OWNER("try\n return\nfinally\n self.value = Resource()\nend"),1);
    source("boxed_allocation.xl",OWNER("try\n var boxed = 1\ncatch e\n return\nend\nself.value = Resource()"),0);
    source("alias.xl",OWNER("auto bad = self\nself.value = Resource()"),0);
    source("capture.xl",OWNER("auto bad = func() as var\n return self\nend\nself.value = Resource()"),0);
    source("all_fields.xl","class Owner\n private Resource first\n private Resource second\n create()\n self.first = Resource()\n auto bad = self\n self.second = Resource()\n end\nend\n",0);
    source("destroy_is_not_create.xl","class Owner\n private Resource value\n destroy()\n return\n end\nend\n",0);
    source("base.xl","class Base\n protected Resource value\n create()\n self.value = Resource()\n end\nend\nclass Owner extends Base\n private Resource own\n create()\n base()\n self.own = Resource()\n end\nend\n",1);
    source("missing_base.xl","class Base\n protected Resource value\n create()\n self.value = Resource()\n end\nend\nclass Owner extends Base\n create()\n return\n end\nend\n",0);
    source("exposing_base.xl","func publish(var value)\nend\nclass Base\n create()\n publish(self)\n end\nend\nclass Owner extends Base\n private Resource value\n create()\n base()\n self.value = Resource()\n end\nend\n",0);
    source("later_base.xl","func publish(var value)\nend\nclass Owner extends Base\n private Resource value\n create()\n base()\n self.value = Resource()\n end\nend\nclass Base\n create()\n publish(self)\n end\nend\n",0);
    source("generic.xl","class Holder<T>\n private T value\n create(T input)\n self.value = input\n end\nend\nauto held = Holder::<Resource>(Resource())\n",1);
    source("constant_condition.xl",OWNER("if not false\n self.value = Resource()\nend"),1);
    source("match.xl",OWNER("match choice\ncase true\n self.value = Resource()\ncase false\n self.value = Resource()\nend"),1);
    source("product.xl","func resources() as (Resource, Resource)\n return Resource(), Resource()\nend\nclass Owner\n private Resource first\n private Resource second\n create()\n self.first, self.second = resources()\n end\nend\n",1);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1)){diagnostics(&unit.parse.diags);return 2;}
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));
    unsigned fields=0;
    for(size_t i=0;i<ir->classFieldCount;++i){
        XL6_IrClassField* field=&ir->classFields[i];
        if(field->initializerAbi!=XL6_IR_INITIALIZER_CONSTRUCTOR_REQUIRED)continue;
        XL6_IrClassField saved=*field;++fields;
        field->initializerAbi=XL6_IR_INITIALIZER_NONE;refused(ir);*field=saved;
        field->initializerAbi=XL6_IR_INITIALIZER_NULL_SENTINEL;refused(ir);*field=saved;
    }
    assert(fields>=4);
    if(!xl6IrLowerModule(ir,&unit.parse.diags)){diagnostics(&unit.parse.diags);return 3;}
    assert(xl6IrVerifyModule(ir,NULL));unsigned stores=0;
    for(size_t f=0;f<ir->functionCount;++f){
        const XL6_IrFunction* function=&ir->functions[f];
        const XL6_IrNode* declaration=xl6IrNodeGet(ir,function->declaration);
        const XL6_IrType* owner=declaration?xl6IrTypeGet(&ir->types,declaration->declarationOwnerType):NULL;
        if(!owner||!owner->name||strncmp(owner->name,"RequiredNative",14))continue;
        if(!strcmp(owner->name,"RequiredNativeRollback"))continue;
        for(size_t i=function->firstInstruction;i<function->firstInstruction+function->instructionCount;++i){
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_RUNTIME_CALL||ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW||
            !ins->immediateText)continue;
        XL6_IrInstruction saved=*ins;char wrong[]="\"notTheRequiredField\"";
        ins->immediateText=wrong;refused(ir);*ins=saved;++stores;
        }
    }
    assert(stores>=4);
    unsigned calls=0;
    for(size_t i=0;i+1<ir->instructionCount;++i){
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->kind!=XL6_IR_INSTR_CALL)continue;
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(ir,ins->targetSymbol);
        const XL6_IrNode* ctor=target?xl6IrNodeGet(ir,target->declaration):NULL;
        if(!ctor||ctor->sourceKind!=XL6_AST_CREATE||!ctor->text||strcmp(ctor->text,"create"))continue;
        const XL6_IrType* owner=ctor?xl6IrTypeGet(&ir->types,ctor->declarationOwnerType):NULL;
        if(!owner||!owner->name||strncmp(owner->name,"RequiredNative",14))continue;
        XL6_IrInstruction* test=&ir->instructions[i+1];
        if(test->kind!=XL6_IR_INSTR_ERROR_TEST||test->operandCount!=1||
            ir->operands[test->firstOperand]!=ins->result)continue;
        XL6_IrInstruction saved=*test;
        test->trueTarget=saved.falseTarget;test->falseTarget=saved.trueTarget;
        refused(ir);*test=saved;++calls;
    }
    assert(calls>0);
    printf("constructor contract: %u semantic/Typed source checks, %u pending fields, %u actual CFG stores, %u rejecting IR mutations\n",sourceChecks,fields,stores,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
