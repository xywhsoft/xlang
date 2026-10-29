#ifndef XLIR_GENERATED_MODULE
#error "A generated asynchronous filesystem module is required"
#endif
#include XLIR_GENERATED_MODULE
int main(void)
{
    int initialized=xlir_module_init();
    if(initialized!=1){
        const xerror* error=xrtGetError();
        fprintf(stderr,"async module failed: %s\n",error ? xrtErrorMessage(error) : "no error");
        for(;error!=NULL;error=xrtErrorCause(error))
            fprintf(stderr,"  %s: %s (system=%lld)\n",xrtErrorOperation(error),
                xrtErrorMessage(error),(long long)xrtErrorSystemCode(error));
        return 1;
    }
    if(xlir_module_prepare_resources(true,true)!=XL6_MODULE_PREPARE_READY)return 2;
    xlir_module_fini();
    return xrtGetError()!=NULL ? 3 : 0;
}
