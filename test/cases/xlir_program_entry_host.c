#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

/* The generic host owns this boundary.  Do not rely on a particular generated
 * module happening to use (and therefore declare) either runtime operation. */
extern xvalue* xrtValueArray(void);
extern void xrtValueRelease(xvalue*);

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}
