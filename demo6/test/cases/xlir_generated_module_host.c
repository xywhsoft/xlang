#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif

    /* Generated XLIR modules intentionally keep their initializer internal.
     * A gate may supply its generated path as a quoted token, or use compiler
     * -include and define XLIR_GENERATED_MODULE_PREINCLUDED.  Both forms keep
     * this host and the module in one translation unit while helper C sources
     * remain separately linkable. */
    #include XLIR_GENERATED_MODULE
#endif

#if defined(XLIR_GENERATED_MODULE_USER_ENTRY)
/* These are host dependencies even when the generated program uses no array
 * construction itself and therefore omits those runtime declarations. */
extern xvalue* xrtValueArray(void);
extern void xrtValueRelease(xvalue*);
#endif

int main(void)
{
#if defined(XLIR_GENERATED_MODULE_USER_ENTRY)
    /* Variadic main receives an actual argument array, as it does through the
     * CLI. NULL is not a valid empty collection at that typed call boundary. */
    xvalue* arguments = xrtValueArray();
    int result;
    if (arguments == NULL) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
