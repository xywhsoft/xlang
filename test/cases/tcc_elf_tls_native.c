#include <stdint.h>
#ifndef TLS_ZERO_ONLY
__thread int elf_public_tls = 123;
static __thread int elf_private_tls = 17;
#else
__thread int elf_public_tls;
static __thread int elf_private_tls;
#endif
static __thread unsigned char elf_aligned_tls[128] __attribute__((aligned(64)));
int elf_tls_read(void)
{ return elf_public_tls + elf_private_tls + elf_aligned_tls[0]; }
void elf_tls_write(int value)
{ elf_public_tls = value; elf_private_tls = value + 1; elf_aligned_tls[0] = 3; }
uintptr_t elf_tls_aligned(void) { return (uintptr_t)elf_aligned_tls; }
#ifdef TLS_IMPORTED
extern __thread int elf_host_tls;
int elf_tls_imported_read(void) { return elf_host_tls; }
void elf_tls_imported_write(int value) { elf_host_tls=value; }
#endif
#ifdef TLS_EXECUTABLE_MAIN
int main(void)
{
    if (elf_tls_read()!=140 || (elf_tls_aligned()&63)) return 1;
    elf_tls_write(42);
    return elf_tls_read()==88 ? 0:2;
}
#endif
