/* Publisher-only signing helper. Private signing operations never enter the updater. */
#define XRT_MODULE_CRYPTO_ED25519_SIGN
#define XRT_MODULE_CRYPTO_ED25519_VERIFY
#define XRT_MODULE_FILE
#define XRT_MODULE_FILE_WHOLE
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#endif
static int hex(const char* s,unsigned char* b,size_t n){if(strlen(s)!=n*2)return 0;for(size_t i=0;i<n;i++){unsigned v;if(sscanf(s+i*2,"%2x",&v)!=1)return 0;b[i]=(unsigned char)v;}return 1;}
static int execute(int argc,char** argv){unsigned char seed[32],pub[32],sig[64];unsigned char* key=NULL,*data=NULL;size_t ks=0,ds=0;int ok=0;
    if(argc==3&&!strcmp(argv[1],"public")){key=xrtFileReadAllLimit(argv[2],32,&ks);if(key&&ks==32&&xrtEd25519Public(key,pub)){for(int i=0;i<32;i++)printf("%02x",pub[i]);puts("");ok=1;}}
    else if(argc==4&&!strcmp(argv[1],"sign")){key=xrtFileReadAllLimit(argv[2],32,&ks);data=xrtFileReadAllLimit(argv[3],16*1024*1024,&ds);if(key&&ks==32&&data){memcpy(seed,key,32);if(xrtEd25519Sign(seed,data,ds,sig)){for(int i=0;i<64;i++)printf("%02x",sig[i]);puts("");ok=1;}memset(seed,0,sizeof(seed));}}
    else if(argc==5&&!strcmp(argv[1],"verify")){data=xrtFileReadAllLimit(argv[3],16*1024*1024,&ds);ok=data&&hex(argv[2],pub,32)&&hex(argv[4],sig,64)&&xrtEd25519Verify(pub,data,ds,sig);}
    else fprintf(stderr,"update-crypto public SEED | sign SEED PAYLOAD | verify PUBLIC PAYLOAD SIGNATURE\n");
    if(!ok&&xrtGetError())fprintf(stderr,"%s\n",xrtErrorMessage(xrtGetError()));if(key)memset(key,0,ks);xrtFree(key);xrtFree(data);return ok?0:1;
}
#ifdef _WIN32
int main(void){int argc;wchar_t** w=CommandLineToArgvW(GetCommandLineW(),&argc);if(!w)return 1;char** argv=calloc((size_t)argc,sizeof(*argv));if(!argv){LocalFree(w);return 1;}int ok=1;for(int i=0;i<argc;i++){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w[i],-1,NULL,0,NULL,NULL);argv[i]=n?malloc((size_t)n):NULL;if(!argv[i]){ok=0;break;}WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w[i],-1,argv[i],n,NULL,NULL);}LocalFree(w);int code=ok?execute(argc,argv):1;for(int i=0;i<argc;i++)free(argv[i]);free(argv);return code;}
#else
int main(int argc,char** argv){return execute(argc,argv);}
#endif
