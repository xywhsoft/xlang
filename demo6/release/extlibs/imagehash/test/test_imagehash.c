#include <stdio.h>
#include <string.h>

int xihash_available(void);
const char* xihash_ahash(const char* path);
const char* xihash_dhash(const char* path);
const char* xihash_phash(const char* path);
const char* xihash_last_error(void);
int xihash_hamming(const char* left, const char* right);
int xihash_valid(const char* hash);

int main(void)
{
    const char* imagePath = "release/examples/extlibs/assets/tiny.png";
    const char* a;
    const char* d;
    const char* p;

    if ( !xihash_available() ) return 1;
    a = xihash_ahash(imagePath);
    d = xihash_dhash(imagePath);
    p = xihash_phash(imagePath);
    if ( !xihash_valid(a) || !xihash_valid(d) || !xihash_valid(p) ) return 2;
    if ( xihash_hamming(a, a) != 0 || xihash_hamming(a, d) < 0 ) return 3;
    if ( xihash_valid("not-a-hash") || xihash_hamming("not-a-hash", a) != -1 ) return 4;
    if ( strcmp(xihash_last_error(), "invalid hash string") != 0 ) return 5;

    puts("imagehash c smoke ok");
    return 0;
}

#include "../imagehash_native.c"
