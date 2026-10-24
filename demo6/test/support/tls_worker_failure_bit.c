/* Isolated checked boundary unit: a well-typed private source deliberately
 * returns NULL WITHOUT a diagnostic. This is not a TLS/network simulation. */
#include "src/runtime/xl6_tls_data_internal.h"
#include <stdio.h>
xfuture* xl6TlsWorkerSourceV1(void* receiver,int64_t argument,unsigned operation)
{(void)receiver;(void)argument;(void)operation;return NULL;}
int main(void)
{
    for(unsigned operation=0;operation<4;operation++){
        xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return 2;
        xfuture* result=operation==0?xl6TlsStreamPeekAsyncV1(NULL,-1):operation==1?xl6TlsStreamConsumeAsyncV1(NULL,0):
            operation==2?xl6TlsStreamSendBoundAsyncV1(NULL,0):xl6TlsStreamProtocolAsyncV1(NULL);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        bool failed=!result&&!outcome.ok&&!outcome.error;xrtErrorFree(outcome.error);
        if(!failed)return 1;
    }
    puts("TLS Worker checked boundary: four diagnosticless failures preserved");return 0;
}
