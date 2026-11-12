/* Error allocation deliberately fails. The explicit failed carrier must still
 * reach the enclosing language boundary; a NULL diagnostic is not success. */
#include <__xl_runtime/xl6_c_integer_arithmetic.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
static const char *lastKind;
xstrview xrtStrView(cstr text)
{ xstrview view={text,strlen(text)};return view; }
xerror* xl6ErrorCreateV1(const char* kind,xstrview text,int32_t line,int32_t column)
{ assert(text.Size && line==7 && column==9);lastKind=kind;return NULL; }
void xrtSetErrorTake(xerror* error)
{ assert(!error);++failures; }
int main(void)
{
    int value=73;unsigned unsignedValue=99;
    assert(xl6CIntArithmeticV1(20,22,XL6_C_INTEGER_ADD,7,9,&value) && value==42 && !failures);
    assert(!xl6CIntArithmeticV1(INT_MAX,1,XL6_C_INTEGER_ADD,7,9,&value) && value==42);
    assert(failures==1 && !strcmp(lastKind,"OverflowError"));
    assert(!xl6CUIntArithmeticV1(0,1,XL6_C_INTEGER_SUB,7,9,&unsignedValue) && unsignedValue==99);
    assert(failures==2 && !strcmp(lastKind,"OverflowError"));
    assert(!xl6CIntArithmeticV1(1,0,XL6_C_INTEGER_DIV,7,9,&value) && value==42);
    assert(failures==3 && !strcmp(lastKind,"ZeroDivisionError"));
    assert(!xl6CIntShiftV1(1,UINT64_C(4294967296),false,7,9,&value) && value==42);
    assert(failures==4 && !strcmp(lastKind,"RangeError"));
    assert(!xl6CIntShiftV1(1,(uint64_t)-1,true,7,9,&value) && value==42);
    assert(failures==5 && !strcmp(lastKind,"RangeError"));
    puts("Checked C integer error allocation failure remains an explicit failure");
    return 0;
}
