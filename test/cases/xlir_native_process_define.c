#include <stdio.h>
#include <string.h>
#ifndef XL6_NATIVE_DEFINE
#error Missing actual transported C string definition
#endif
int main(int argc, char** argv)
{
    if (argc != 2 || strcmp(XL6_NATIVE_DEFINE,argv[1])) return 1;
    puts("actual C preprocessor string and process argument agree"); return 0;
}
