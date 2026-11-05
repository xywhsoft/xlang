/* GCC -fPIC emits an undefined _GLOBAL_OFFSET_TABLE_ symbol, while the live
 * R_X86_64_REX_GOTPCRELX relocation names native_elf_gc_data instead. The GOT
 * itself is synthesized after section GC by TinyCC. */
extern int native_elf_gc_data;
extern int native_elf_gc_missing(void);
#if defined(REAL_GOT_CONFLICT)
/* A genuine user global absolute definition must remain an error. */
__asm__(".globl _GLOBAL_OFFSET_TABLE_\n.set _GLOBAL_OFFSET_TABLE_,0");
#endif
int native_elf_gc_pic_read(void) { return native_elf_gc_data; }
int native_elf_gc_dead(void) { return native_elf_gc_missing(); }
