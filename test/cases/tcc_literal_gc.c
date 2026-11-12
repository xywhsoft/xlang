/* CPU JIT plus object/final-image inspection. Emitted PE images are not run. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"

static int checks, failures;
static char diagnostics[4096];
static const char *library_root;
static const unsigned char marker[] = "UNREACHABLE_LITERAL_PAYLOAD_";

static void check(int ok, const char *name)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL %s: %s\n", name, diagnostics); }
}
static void error_sink(void *unused, const char *message)
{
    size_t used = strlen(diagnostics);
    (void)unused;
    if (used < sizeof diagnostics - 2)
        snprintf(diagnostics + used, sizeof diagnostics - used, "%s\n", message);
}
static TCCState *make_state(int output, int gc)
{
    TCCState *s = tcc_new();
    diagnostics[0] = 0;
    tcc_set_error_func(s, NULL, error_sink);
    tcc_set_lib_path(s, library_root);
    check(tcc_set_options(s, gc ?
        "-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=entry,-e,entry" :
        "-nostdlib -ffunction-sections -fdata-sections -Wl,-e,entry") == 0, "literal options accepted");
    check(tcc_set_output_type(s, output) == 0, "literal output selected");
    return s;
}
static int contains(TCCState *s, const unsigned char *bytes, size_t length)
{
    int i;
    for (i = 1; i < s->nb_sections; ++i) {
        Section *section = s->sections[i];
        unsigned long at;
        if (!(section->sh_flags & SHF_ALLOC) || !section->data || section->data_offset < length)
            continue;
        for (at = 0; at <= section->data_offset - length; ++at)
            if (!memcmp(section->data + at, bytes, length)) return 1;
    }
    return 0;
}
static int literal_sections(TCCState *s)
{
    int i, count = 0;
    for (i = 1; i < s->nb_sections; ++i) {
        Section *section = s->sections[i];
        if ((section->sh_flags & SHF_ALLOC) && section->data_offset &&
            strstr(section->name, ".__tcc_literal.")) ++count;
    }
    return count;
}
static unsigned long literal_storage(TCCState *s)
{
    int i;
    unsigned long bytes = 0;
    for (i = 1; i < s->nb_sections; ++i) {
        Section *section = s->sections[i];
        if ((section->sh_flags & SHF_ALLOC) && strstr(section->name, ".__tcc_literal."))
            bytes += section->data_offset;
    }
    return bytes;
}
/* Public relocation releases section metadata after copying. Inspect final
 * reachability before that copy; this no-libc program adds no runtime roots. */
static void collect(TCCState *s)
{
    tcc_enter_state(s);
    tcc_gc_sections(s, "entry", 0);
    tcc_exit_state(s);
}
static char *program(void)
{
    static char source[20000];
    size_t used;
    used = snprintf(source, sizeof source,
        "static const char* dead(void){return \"%s", marker);
    memset(source + used, 'q', 16000); used += 16000;
    snprintf(source + used, sizeof source - used,
        "\";}\n"
        "static double dead_number(void){return 0x1.56789abcdefp+20;}\n"
        "static const char *const tail=\"prefix_tail\"+7;\n"
        "__attribute__((section(\".literal_user\"),aligned(32))) static const unsigned char explicit_data[]={9,8,7};\n"
        "static const char* live(void){return \"a\\0\" \"b\";}\n"
        "int entry(void){const char*s=live(); float f=1.25f; double d=2.5; long double l=3.75L;\n"
        "union{double d; unsigned long long bits;} z; z.d=-0.0;\n"
#ifdef TCC_TARGET_PE
        "const unsigned short*w=L\"A\\0B\";\n"
#else
        "const int*w=L\"A\\0B\";\n"
#endif
        "return s[0]=='a' && s[1]==0 && s[2]=='b' && w[0]=='A' && w[1]==0 && w[2]=='B'\n"
        "&& tail[0]=='t' && f==1.25f && d==2.5 && l==3.75L && (z.bits>>63)==1\n"
        "&& explicit_data[2]==7 && (((unsigned long long)explicit_data)&31)==0 ? 42 : 0;}\n");
    return source;
}
static void test_jit(int gc)
{
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, gc);
    union { double value; unsigned char bytes[8]; } dead;
    int (*entry)(void);
    Section *custom;
    dead.value = 0x1.56789abcdefp+20;
    check(tcc_compile_string(s, program()) == 0, "literal program compiled");
    check(contains(s, marker, sizeof marker - 1), "dead string really emitted before collection");
    check(contains(s, dead.bytes, sizeof dead.bytes), "dead float really emitted before collection");
    collect(s);
    custom = have_section(s, ".literal_user");
    check(custom && (custom->sh_flags & SHF_ALLOC) && custom->data_offset == 3,
        "explicit user section remains a separate named section");
    check(contains(s, marker, sizeof marker - 1) == !gc, "dead literal follows its function reachability");
    check(contains(s, dead.bytes, sizeof dead.bytes) == !gc, "dead float follows its function reachability");
    check(literal_sections(s) == 0, "surviving literal fragments coalesced after final reachability");
    check(tcc_relocate(s) == 0, "literal JIT relocated");
    entry = tcc_get_symbol(s, "entry");
    check(entry && entry() == 42, "NUL/wide/interior pointer/alignment/float values preserved");
    tcc_delete(s);
}
static void test_objects(const char *directory, int gc)
{
    char first[2048], second[2048];
    TCCState *s;
    int (*entry)(void);
    snprintf(first, sizeof first, "%s/literal-first.o", directory);
    snprintf(second, sizeof second, "%s/literal-second.o", directory);
    s = make_state(TCC_OUTPUT_OBJ, 1);
    check(tcc_compile_string(s, "const char* first(void){return \"A\\0Z\";} static const char*unused(void){return \"UNREACHABLE_LITERAL_PAYLOAD_1\";}") == 0,
        "first object compiled");
    check(tcc_output_file(s, first) == 0, "first object emitted");
    check(contains(s, marker, sizeof marker - 1), "object-only emission does not discard literal storage");
    tcc_delete(s);
    s = make_state(TCC_OUTPUT_OBJ, 1);
    check(tcc_compile_string(s, "extern const char*first(void); static const char*second(void){return \"longer\\0Y\";} int entry(void){volatile double n=3.75;const char*q=\"Q\";return first()[2]=='Z' && second()[7]=='Y' && q[0]=='Q' && n==3.75 && \"x\"[0]=='x' ? 59 : 0;}") == 0,
        "second object with repeated compiler literal ordinals compiled");
    check(tcc_output_file(s, second) == 0, "second object emitted"); tcc_delete(s);
    s = make_state(TCC_OUTPUT_MEMORY, gc);
    /* The consumer need not ask for data sections: the object already has them. */
    check(tcc_set_options(s, "-fno-data-sections") == 0, "consumer data splitting disabled");
    check(tcc_add_file(s, first) == 0 && tcc_add_file(s, second) == 0, "literal objects loaded");
    collect(s);
    check(contains(s, marker, sizeof marker - 1) == !gc, "object literal garbage collection obeys consumer mode");
    check(literal_sections(s) == 0, "object literals compact without final section proliferation");
    check(tcc_relocate(s) == 0, "literal objects relocated");
    entry = tcc_get_symbol(s, "entry");
    check(entry && entry() == 59, "same ordinal object literals and interior offsets preserved");
    tcc_delete(s);
}
static void test_final_image(const char *directory)
{
    char source[10000], path[2048];
    size_t used = 0;
    TCCState *s = make_state(TCC_OUTPUT_EXE, 1);
    int i;
    used += snprintf(source + used, sizeof source - used, "static const char*const words[192]={");
    for (i = 0; i < 192; ++i)
        used += snprintf(source + used, sizeof source - used, "%s\"word-%d\"", i ? "," : "", i);
    snprintf(source + used, sizeof source - used,
        "}; int entry(void){int i,sum=0; for(i=0;i<192;++i)sum+=words[i][0];return sum;}");
    check(tcc_compile_string(s, source) == 0, "many live literal source compiled");
    snprintf(path, sizeof path, "%s/literals-native.bin", directory);
    check(tcc_output_file(s, path) == 0, "many live literal image emitted");
    check(literal_sections(s) == 0, "final image has no per-literal fragment sections");
#ifndef TCC_TARGET_PE
    check(s->nb_sections < 32, "final ELF section headers stay bounded with 192 literals");
#endif
    tcc_delete(s);
}
#ifdef TCC_LITERAL_GC_QUALIFY
static void test_large_function(void)
{
    const int count = 35000;
    const int remainder = count % 26;
    const int expected = (count / 26) * (26 * 'A' + 325)
        + remainder * 'A' + remainder * (remainder - 1) / 2;
    char *source = tcc_malloc((unsigned long)count * 64 + 256);
    size_t used = 0;
    int at, result, (*entry)(void);
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, 1);
    used += sprintf(source + used, "int entry(void){const char*volatile p;int sum=0;\n");
    for (at = 0; at < count; ++at)
        used += sprintf(source + used, "p=\"%c-literal-%d\";sum+=p[0];\n", 'A' + at % 26, at);
    sprintf(source + used, "return sum;}\n");
    result = tcc_compile_string(s, source);
    check(result == 0, "35000-literal function compiled without section-index exhaustion");
    tcc_free(source);
    if (result) { tcc_delete(s); return; }
    check(s->nb_sections < 64 && literal_sections(s) == 1,
        "one function's literal count does not multiply section metadata");
    check(literal_storage(s) == 548890, "large function stores each unique literal once, including its NUL");
    result = tcc_relocate(s);
    check(result == 0, "large function JIT relocated");
    entry = result ? NULL : tcc_get_symbol(s, "entry");
    check(entry && entry() == expected, "35000 varying literal values contribute the expected checksum");
    tcc_delete(s);
}
static void test_static_initializer_owner(void)
{
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, 1);
    Section *keep;
    int relocated;
    int (*entry)(void);
    check(tcc_compile_string(s,
        "extern const char*pinned;"
        "static const char*dead(void){static const char*p __attribute__((section(\"keep\")))=\"PINNED\";"
        "return \"UNREACHABLE_LITERAL_PAYLOAD_OWNER\";}"
        "int entry(void){return pinned[0]=='P'?71:0;}") == 0,
        "static initializer retained independently of its enclosing function compiled");
    /* Export the local registration slot as a normal object definition; the
     * live code references its storage, not the enclosing function. */
    keep = have_section(s, "keep");
    check(keep && keep->data_offset == PTR_SIZE, "static registration storage emitted");
    if (!keep) { tcc_delete(s); return; }
    set_elf_sym(s->symtab, 0, PTR_SIZE, ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT),
        0, keep->sh_num, "pinned");
    collect(s);
    check(!contains(s, marker, sizeof marker - 1),
        "retained static initializer does not pin unrelated literals from a dead function");
    relocated = tcc_relocate(s);
    check(relocated == 0, "independent static initializer JIT relocated");
    entry = relocated ? NULL : tcc_get_symbol(s, "entry");
    check(entry && entry() == 71, "retained static initializer literal remains live");
    tcc_delete(s);
}
static void test_large_initializer(void)
{
    const int count = 35000, remainder = count % 26;
    const int expected = (count / 26) * (26 * 'A' + 325)
        + remainder * 'A' + remainder * (remainder - 1) / 2;
    char *source = tcc_malloc((unsigned long)count * 40 + 256);
    size_t used = 0;
    int at, result, (*entry)(void);
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, 1);
    used += sprintf(source + used, "static const char*const words[]={");
    for (at = 0; at < count; ++at)
        used += sprintf(source + used, "%s\"%c-global-%d\"", at ? "," : "", 'A' + at % 26, at);
    sprintf(source + used, "};int entry(void){int i,sum=0;for(i=0;i<%d;++i)sum+=words[i][0];return sum;}", count);
    result = tcc_compile_string(s, source);
    check(result == 0, "35000-literal incomplete-array initializer compiled");
    tcc_free(source);
    if (result) { tcc_delete(s); return; }
    check(s->nb_sections < 64 && literal_sections(s) == 1,
        "one data initializer's literal count does not multiply section metadata");
    check(literal_storage(s) == 513890,
        "incomplete-array sizing pass does not duplicate literal storage");
    result = tcc_relocate(s);
    check(result == 0, "large initializer JIT relocated");
    entry = result ? NULL : tcc_get_symbol(s, "entry");
    check(entry && entry() == expected, "35000 initializer pointers preserve their expected checksum");
    tcc_delete(s);
}
/* Exercise the existing parser-protected allocator error path, not a new
 * public promise that a failed final link can be resumed in place. */
#undef _tcc_error
static long allocator_live;
static int allocator_armed, allocator_calls, allocator_fail = -1, allocator_failed;
static void *controlled_allocator(void *ptr, unsigned long size)
{
    void *next;
    if (size && allocator_armed) {
        if (allocator_calls++ == allocator_fail) {
            allocator_armed = 0;
            allocator_failed = 1;
            _tcc_error("fixture literal compaction allocation failure");
        }
    }
    next = default_reallocator(ptr, size);
    if (!ptr && size) ++allocator_live;
    else if (ptr && !size) --allocator_live;
    return next;
}
static int allocation_prefix(int fail)
{
    TCCState *s;
    int observed;
    allocator_live = 0;
    allocator_calls = allocator_failed = 0;
    allocator_fail = fail;
    tcc_set_realloc(controlled_allocator);
    s = make_state(TCC_OUTPUT_MEMORY, 0);
    check(tcc_compile_string(s, program()) == 0, "allocation-prefix input compiled");
    tcc_enter_state(s);
    s->error_set_jmp_enabled = 1;
    if (!setjmp(s->error_jmp_buf)) {
        allocator_armed = 1;
        gc_pack_literals(s);
        allocator_armed = 0;
    }
    s->error_set_jmp_enabled = 0;
    tcc_exit_state(s);
    observed = allocator_calls;
    check(allocator_failed == (fail >= 0), "every injected compaction allocation failure delivered");
    check(!nb_stk_data, "temporary cleanup entries popped on success or nonlocal failure");
    /* The parser's ordinary outer cleanup frees its registry after an error. */
    dynarray_reset(&stk_data, &nb_stk_data);
    tcc_delete(s);
    check(!allocator_live, "compaction failure and state teardown retain no allocator blocks");
    tcc_set_realloc(NULL);
    return observed;
}
static void test_allocation_prefixes(void)
{
    int at, count = allocation_prefix(-1);
    check(count > 0, "real compaction allocations observed before failure injection");
    for (at = 0; at < count; ++at) allocation_prefix(at);
}
static void test_parent_cleanup(void)
{
    TCCState *s = make_state(TCC_OUTPUT_MEMORY, 0);
    void *parent;
    check(tcc_compile_string(s, program()) == 0, "nested cleanup input compiled");
    tcc_enter_state(s);
    parent = tcc_malloc(1);
    stk_push(&parent);
    gc_pack_literals(s);
    check(nb_stk_data == 1 && stk_data[0] == &parent,
        "compaction preserves its caller's pending cleanup entry");
    tcc_free(parent);
    stk_pop();
    dynarray_reset(&stk_data, &nb_stk_data);
    tcc_exit_state(s);
    tcc_delete(s);
}
#endif
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    library_root = argv[1];
    test_jit(0); test_jit(1);
    test_objects(argv[2], 0); test_objects(argv[2], 1);
    test_final_image(argv[2]);
    check(!stk_data && !nb_stk_data, "literal final linking releases temporary error-stack storage");
#ifdef TCC_LITERAL_GC_QUALIFY
    test_large_function();
    test_large_initializer();
    test_static_initializer_owner();
    test_allocation_prefixes();
    test_parent_cleanup();
#endif
    printf("LITERAL_GC_CHECKS=%d FAILURES=%d\n", checks, failures);
    return failures ? 1 : 0;
}
