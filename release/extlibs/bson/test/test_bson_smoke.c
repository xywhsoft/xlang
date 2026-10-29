#include "../bson_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char* oid = xbson_gen_oid();

    if ( oid == NULL || strlen(oid) != 24u || xbson_oid_time(oid) <= 0 ||
         xbson_oid_time("not-an-object-id") != -1 ) {
        fputs("test_bson_smoke: ObjectId validation failed\n", stderr);
        return 1;
    }
    xrtFree(oid);
    puts("test_bson_smoke: ok");
    return 0;
}
