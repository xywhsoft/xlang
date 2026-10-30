#include "test_numeric_arg.h"
/* Test executable for the response helper's argv, stderr and exit protocol.
 * Not a compiler or a production provider. Real GCC linkage is tested by the
 * ordinary DLL build separately. Paths in this test include spaces. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char** argv)
{
    assert(argc==2 && argv[1][0]=='@' && argv[1][1]);
    FILE* source=fopen(argv[1]+1,"rb");assert(source);
    size_t size=0;while(fgetc(source)!=EOF)++size;
    assert(!ferror(source) && !fclose(source));
    printf("response-probe bytes=%zu\n",size);fputs("response-probe stderr\n",stderr);
    const char* code=getenv("XL6_DLL_RESPONSE_PROBE_EXIT");assert(code);
    return xlTestIntArg(code);
}
