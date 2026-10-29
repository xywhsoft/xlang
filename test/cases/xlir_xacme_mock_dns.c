/* Local fixture transport only; production XACME still owns all ACME/TLS. */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET TestSocket;
#define testClose closesocket
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
typedef int TestSocket;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define testClose close
#endif
extern void* xrtMalloc(size_t size);
extern void xrtFree(void* memory);
bool test_xacme_dns(const char* fqdn,const char* txt,bool add)
{
    const char* base=getenv("XACME_MOCK_CHALL");const char* port=base?strrchr(base,':'):NULL;
    if(!port||!fqdn||!txt||strlen(fqdn)>600||strlen(txt)>200||strpbrk(fqdn,"\"\\")||strpbrk(txt,"\"\\"))return false;
    char body[1024],request[2048],response[256];
    int bodySize=snprintf(body,sizeof(body),"{\"host\":\"%s\",\"value\":\"%s\"}",fqdn,txt);
    int length=snprintf(request,sizeof(request),"POST /%s-txt HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",add?"set":"clear",bodySize,body);
    if(bodySize<0||length<0||(size_t)length>=sizeof(request))return false;
    TestSocket sock=socket(AF_INET,SOCK_STREAM,0);if(sock==INVALID_SOCKET)return false;
    struct sockaddr_in address;memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;address.sin_port=htons((unsigned short)atoi(port+1));address.sin_addr.s_addr=htonl(0x7f000001u);
#ifdef _WIN32
    DWORD timeout=3000;setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
#else
    struct timeval timeout={3,0};setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
#endif
    if(connect(sock,(const struct sockaddr*)&address,sizeof(address))==SOCKET_ERROR){testClose(sock);return false;}
    int written=0;
    while(written<length){int n=(int)send(sock,request+written,length-written,0);if(n<=0){testClose(sock);return false;}written+=n;}
    int received=0;
    while(received<12){int n=(int)recv(sock,response+received,(int)sizeof(response)-received,0);if(n<=0){testClose(sock);return false;}received+=n;}
    bool ok=!memcmp(response+9,"200",3);testClose(sock);return ok;
}
static void* testReadPem(const char* path)
{
    FILE* file=path?fopen(path,"rb"):NULL;if(!file)return NULL;
    if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}long n=ftell(file);rewind(file);
    if(n<=0||n>65536){fclose(file);return NULL;}char* result=xrtMalloc((size_t)n+1);
    if(result&&fread(result,1,(size_t)n,file)!=(size_t)n){xrtFree(result);result=NULL;}
    fclose(file);if(result)result[n]=0;return result;
}
void* test_xacme_ca(void){return testReadPem(getenv("XACME_MOCK_CA"));}
void* test_xacme_rollover_key(void)
{
    return testReadPem(getenv("XACME_ROLLOVER_KEY"));
}
