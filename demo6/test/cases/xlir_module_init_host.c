#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
