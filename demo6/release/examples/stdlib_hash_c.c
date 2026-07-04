#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_hash(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "hash=fail\n");
        exit(1);
    }
}

int main(void)
{
    const char* text = "xlang";
    size_t len = strlen(text);
    uint32 id32;
    uint64 id64;
    uint32 seeded32;
    uint64 seeded64;
    uint64 micro64;
    uint64 microSeeded64;
    uint64 nano64;
    uint64 nanoSeeded64;

    xrtInit();

    id32 = xrtHash32((ptr)text, len);
    id64 = xrtHash64((ptr)text, len);
    seeded32 = xrtHash32_WithSeed((ptr)text, len, 123u);
    seeded64 = xrtHash64_WithSeed((ptr)text, len, 123ull);
    micro64 = xrtHash64_Micro((ptr)text, len);
    microSeeded64 = xrtHash64_Micro_WithSeed((ptr)text, len, 123ull);
    nano64 = xrtHash64_Nano((ptr)text, len);
    nanoSeeded64 = xrtHash64_Nano_WithSeed((ptr)text, len, 123ull);

    require_hash(id32 == xrtHash32((ptr)"xlang", 5u));
    require_hash(id64 == xrtHash64((ptr)"xlang", 5u));
    require_hash(seeded32 == xrtHash32_WithSeed((ptr)"xlang", 5u, 123u));
    require_hash(seeded64 == xrtHash64_WithSeed((ptr)"xlang", 5u, 123ull));
    require_hash(micro64 == xrtHash64_Micro((ptr)"xlang", 5u));
    require_hash(microSeeded64 == xrtHash64_Micro_WithSeed((ptr)"xlang", 5u, 123ull));
    require_hash(nano64 == xrtHash64_Nano((ptr)"xlang", 5u));
    require_hash(nanoSeeded64 == xrtHash64_Nano_WithSeed((ptr)"xlang", 5u, 123ull));

    require_hash(id32 != xrtHash32((ptr)"xlang!", 6u));
    require_hash(id64 != xrtHash64((ptr)"xlang!", 6u));
    require_hash(seeded32 != xrtHash32_WithSeed((ptr)"xlang", 5u, 124u));
    require_hash(seeded64 != xrtHash64_WithSeed((ptr)"xlang", 5u, 124ull));

    printf("hash=ok\n");
    xrtUnit();
    return 0;
}
