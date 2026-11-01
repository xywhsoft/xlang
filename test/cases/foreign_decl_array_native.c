#include "foreign_decl_array.h"
#include <stdlib.h>
foreign_decl_matrix foreign_decl_matrix_data(void)
{
    foreign_decl_matrix value = {{0}};
    for (int i = 0; i < 16; ++i) value.m[i] = (float)(i + 1);
    return value;
}
int foreign_decl_matrix_sum(foreign_decl_matrix value)
{
    float result = 0;
    for (int i = 0; i < 16; ++i) result += value.m[i];
    return (int)result;
}
void foreign_decl_array_expect(int actual, int expected)
{
    if (actual != expected) abort();
}
