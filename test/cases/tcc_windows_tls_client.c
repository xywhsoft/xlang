#include <stdio.h>
int native_tls_run(const char*path);int native_tls_checks(void);int native_tls_failures(void);
int main(int argc,char**argv){if(argc!=2)return 2;int result=native_tls_run(argv[1]);printf("TLS_BODY_CHECKS=%d FAILURE_MASK=%x RESULT=%d\n",native_tls_checks(),native_tls_failures(),result);return result;}
