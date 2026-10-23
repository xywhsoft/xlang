#include "protocol/diagnostic_paths.h"
#include <string.h>
static int integer(xsha256* state,uint32_t value)
{
 unsigned char bytes[4]={(unsigned char)(value>>24),(unsigned char)(value>>16),(unsigned char)(value>>8),(unsigned char)value};
 return xrtSha256Update(state,bytes,4);
}
int xlsDiagnosticPathsDigest(const xvalue* paths,char digest[65])
{
 if(!digest) return 0;
 digest[0]=0;
 if(!paths || xrtValueType(paths)!=XVALUE_ARRAY || xrtValueCount(paths)>64u*(512u+2048u)) return 0;
 xsha256 state;xrtSha256Init(&state);
 static const char tag[]="xlang-diagnostic-paths-v1";
 if(!xrtSha256Update(&state,tag,sizeof(tag)) || !integer(&state,(uint32_t)xrtValueCount(paths))) return 0;
 for(size_t i=0;i<xrtValueCount(paths);++i) {
  xstrview path={0};
  if(!xrtValueGetString(xrtValueArrayGet(paths,i),&path) || !path.Size || path.Size>65535 ||
     memchr(path.Data,0,path.Size) || !xrtUtf8Valid(path,NULL) || !integer(&state,(uint32_t)path.Size) || !xrtSha256Update(&state,path.Data,path.Size)) return 0;
 }
 unsigned char hash[32];static const char hex[]="0123456789abcdef";
 if(!xrtSha256Final(&state,hash)) return 0;
 for(size_t i=0;i<32;++i) {digest[2*i]=hex[hash[i]>>4];digest[2*i+1]=hex[hash[i]&15];}
 digest[64]=0;return 1;
}
