#include "src/type/xl6_foreign_decl.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XL6_AstNode one={0},two={0},zero={0},minus={0},minimum={0};
    one.kind=two.kind=zero.kind=XL6_AST_INT;one.text="1";two.text="2";zero.text="0";
    XL6_AstNode* negativeChildren[]={&one};minus.kind=XL6_AST_UNARY;minus.text="-";
    minus.children=negativeChildren;minus.childCount=1;
    minimum.kind=XL6_AST_STRING;minimum.text="\"integer_min\"";
    XL6_AstNode* slots[]={&minus,&one,&two};
    XL6_AstNode outputs={0};outputs.kind=XL6_AST_FIELD_ATTR;outputs.text="c_outputs";
    outputs.children=slots;outputs.childCount=3;
    XL6_AstNode* resultChildren[]={&minimum,&zero};
    XL6_AstNode result={0};result.kind=XL6_AST_FIELD_ATTR;result.text="c_result";
    result.children=resultChildren;result.childCount=2;
    XL6_AstNode* statusChildren[]={&zero};
    XL6_AstNode status={0};status.kind=XL6_AST_FIELD_ATTR;status.text="c_status";
    status.children=statusChildren;status.childCount=1;
    XL6_AstNode* attributes[]={&outputs,&result,&status};
    XL6_AstNode attrs={0};attrs.kind=XL6_AST_FIELD_ATTR;attrs.text="c_call";
    attrs.children=attributes;attrs.childCount=2;
    XL6_AstNode* functionChildren[]={&attrs};
    XL6_AstNode function={0};function.kind=XL6_AST_EXTERN_FUNC;
    function.children=functionChildren;function.childCount=1;function.flags=XL6_AST_FLAG_FOREIGN_NO_RAISE;
    XL6_ForeignCallFacts facts={0};uint32 decoded[64];size_t count=0;
    assert(xl6ForeignFunctionRead(&function,&facts));
    assert(xl6ForeignValueOutputHasReturn(facts.valueOutput) && facts.outputIndexPlusOne==2);
    assert(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INTEGER_MIN && !facts.successCode);
    assert(xl6ForeignMultiOutputRead(&outputs,decoded,&count) && count==3 && decoded[0]==UINT32_MAX);
    assert(xl6ForeignMultiOutputSlotsValid(&facts,1,0,decoded,count));
    assert(xl6ForeignPhysicalOutputSlots(&function,&facts,decoded,&count) && count==2 && decoded[0]==1 && decoded[1]==2);
    attrs.childCount=3;assert(!xl6ForeignFunctionRead(&function,&facts));attrs.childCount=2;
    slots[0]=&one;slots[1]=&minus;assert(!xl6ForeignFunctionRead(&function,&facts));
    slots[0]=&minus;slots[1]=&one;slots[2]=&one;assert(!xl6ForeignFunctionRead(&function,&facts));slots[2]=&two;
    for(unsigned bit=16;bit<=128;bit<<=1)assert(!xl6ForeignValueOutputBitsValid(XL6_FOREIGN_VALUE_OUTPUT_MULTI_INT_VALUE|bit));
    outputs.childCount=1;assert(!xl6ForeignFunctionRead(&function,&facts));outputs.childCount=3;
    assert(xl6ForeignFunctionRead(&function,&facts));
    assert(sizeof(XL6_ForeignOutputContract)==3*sizeof(uint32));
    puts("FOREIGN_RETURN_OUTPUT_DECL_OK");return 0;
}
