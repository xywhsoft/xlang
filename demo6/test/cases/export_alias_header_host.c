#include "export_alias.h"

int main(void)
{
    return xl6_alias_add(20, 22) == 42 ? 0 : 1;
}
