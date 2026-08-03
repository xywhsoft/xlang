#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern int xrtSecureRandom(void *data, size_t size);
extern void xrtSecureZero(void *data, size_t size);
extern void xrtClearError(void);

static int is_all_zero(const unsigned char *data, size_t size)
{
    size_t i;

    for ( i = 0; i < size; i++ ) {
        if ( data[i] != 0 ) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    unsigned char first[64] = { 0 };
    unsigned char second[64] = { 0 };

    if ( !xrtSecureRandom(NULL, 0) ) {
        return 1;
    }
    if ( xrtSecureRandom(NULL, 1) ) {
        return 2;
    }
    xrtClearError();

    if ( !xrtSecureRandom(first, sizeof(first)) ||
         !xrtSecureRandom(second, sizeof(second)) ) {
        return 3;
    }
    if ( is_all_zero(first, sizeof(first)) ||
         memcmp(first, second, sizeof(first)) == 0 ) {
        return 4;
    }

    xrtSecureZero(first, sizeof(first));
    if ( !is_all_zero(first, sizeof(first)) ) {
        return 5;
    }

    puts("xrt_secure_random=ok");
    return 0;
}
