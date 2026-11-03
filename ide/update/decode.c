#include "update.h"
#include "LzmaDec.h"
#include <stdlib.h>
#include <string.h>
static void* alloc(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void release(ISzAllocPtr p,void* b){(void)p;free(b);}
int up_decode(const char* source,const char* target,uint64_t limit) {
    FILE* in=up_open(source,"rb"),*out=NULL;int ok=0;CLzmaDec decoder;LzmaDec_Construct(&decoder);ISzAlloc allocator={alloc,release};
    unsigned char header[13],input[65536],output[65536];size_t have=0,offset=0;uint64_t expected=0,total=0;
    if(!in||fread(header,1,13,in)!=13)goto done;
    for(unsigned i=0;i<8;i++)expected|=(uint64_t)header[5+i]<<(8*i);
    uint32_t dict=(uint32_t)header[1]|((uint32_t)header[2]<<8)|((uint32_t)header[3]<<16)|((uint32_t)header[4]<<24);
    if(expected>limit||dict>32u*1024u*1024u||LzmaDec_Allocate(&decoder,header,5,&allocator)!=SZ_OK)goto done;
    if(!up_parents(target)||(out=up_open(target,"wb"))==NULL)goto done;LzmaDec_Init(&decoder);
    for(;;){if(up_cancel)goto done;if(offset==have){have=fread(input,1,sizeof(input),in);offset=0;if(ferror(in))goto done;}
        SizeT used=have-offset,n=(SizeT)((expected-total)<sizeof(output)?expected-total:sizeof(output));ELzmaStatus status;
        SRes result=LzmaDec_DecodeToBuf(&decoder,output,&n,input+offset,&used,total+n==expected?LZMA_FINISH_END:LZMA_FINISH_ANY,&status);
        offset+=used;if(result!=SZ_OK||n>expected-total||fwrite(output,1,n,out)!=n)goto done;total+=n;
        if(status==LZMA_STATUS_FINISHED_WITH_MARK){ok=total==expected&&offset==have&&fgetc(in)==EOF&&!ferror(in);break;}
        if(!used&&!n)break;
    }
done:
    LzmaDec_Free(&decoder,&allocator);if(in)fclose(in);if(out){if(!up_sync(out))ok=0;if(fclose(out))ok=0;}
    if(!ok){up_remove(target);return up_error("LZMA verification/decode failed: %s",source);}return 1;
}
