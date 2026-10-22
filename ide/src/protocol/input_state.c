#include "protocol/input_state.h"
#include <xrt.h>
#include <stdlib.h>
#include <string.h>

int xlsInputStateUri(const char* uri,unsigned char hash[32])
{
 if(!hash) return 0;
 memset(hash,0,32); if(!uri || !*uri) return 0;
 size_t length=strlen(uri);
 return xrtUtf8Valid((xstrview){uri,length},NULL) && xrtSha256(uri,length,hash);
}
static int order(const void* a,const void* b)
{ return memcmp(((const xls_input_state_item*)a)->uri,((const xls_input_state_item*)b)->uri,32); }
static void encode(unsigned char* out,uint32_t value)
{ for(size_t i=0;i<4;++i) out[i]=(unsigned char)(value>>(24-i*8)); }
int xlsInputStateDigest(const xls_input_state_item* items,size_t count,char digest[65])
{
 if(!digest) return 0;
 digest[0]=0; if(count>64 || (count && !items)) return 0;
 static const unsigned char domain[]="xlang-input-state-v1";
 xls_input_state_item sorted[64]; unsigned char bytes[sizeof(domain)+4+64*36],hash[32];
 if(count) { memcpy(sorted,items,count*sizeof(*items)); qsort(sorted,count,sizeof(*sorted),order); }
 memcpy(bytes,domain,sizeof(domain)); size_t at=sizeof(domain); encode(bytes+at,(uint32_t)count); at+=4;
 for(size_t i=0;i<count;++i) {
  if(i && !memcmp(sorted[i-1].uri,sorted[i].uri,32)) return 0;
  memcpy(bytes+at,sorted[i].uri,32); at+=32; encode(bytes+at,(uint32_t)sorted[i].version); at+=4;
 }
 if(!xrtSha256(bytes,at,hash)) return 0;
 static const char hex[]="0123456789abcdef";
 for(size_t i=0;i<32;++i) { digest[2*i]=hex[hash[i]>>4]; digest[2*i+1]=hex[hash[i]&15]; }
 digest[64]=0; return 1;
}
