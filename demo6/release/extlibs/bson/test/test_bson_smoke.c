#include <stdio.h>
#include <string.h>

extern const char* xbson_gen_oid(void);
extern long long xbson_oid_time(const char* hex24);

int main(void)
{
    const char* oid = xbson_gen_oid();

    if ( oid == NULL || strlen(oid) != 24u || xbson_oid_time(oid) <= 0 ||
         xbson_oid_time("not-an-object-id") != 0 ) {
        fputs("test_bson_smoke: ObjectId validation failed\n", stderr);
        return 1;
    }
    puts("test_bson_smoke: ok");
    return 0;
}
