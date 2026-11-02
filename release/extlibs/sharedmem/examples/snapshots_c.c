/* 与 snapshots.xl 同一协议、同一输出；读结果跨所有映射句柄退休而存活。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
#include "../include/sharedmem_native.h"
#ifdef __TINYC__
#include "../sharedmem_native.c"
#endif
#include <stdio.h>
#include <string.h>

static void* connect(const char* name)
{
    void* owner=xshm_create(name,64);
    void* peer=NULL;
    if(owner && xshm_write_int64(owner,1,INT64_C(0x0102030405060708)) &&
        xshm_write(owner,16,"A\0你好",8)) peer=xshm_open(name);
    xshm_close(owner);return peer;
}
int main(void)
{
    uint64_t nonce;
    if(!xrtSecureRandom(&nonce,sizeof(nonce)))return 1;
    char name[96];snprintf(name,sizeof(name),"xl-shm-snapshot-%016llx",(unsigned long long)nonce);
    void* peer=connect(name);
    xbuffer *wire=NULL,*binary=NULL;
    char* text=NULL;
    int result=1;int64_t value=0;
    if(!peer || xshm_size(peer)!=64 || !xshm_read_int64(peer,1,&value) || value!=INT64_C(0x0102030405060708))goto done;
    puts("peer survives owner: true");
    wire=xshm_read(peer,1,8);
    static const unsigned char little[]={8,7,6,5,4,3,2,1};
    if(!wire || xrtBufferView(wire).Size!=8 || memcmp(xrtBufferView(wire).Data,little,8))goto done;
    printf("little endian: ");
    for(size_t i=0;i<8;++i)printf("%02x",((const unsigned char*)xrtBufferView(wire).Data)[i]);
    putchar('\n');
    text=xshm_read_text(peer,16,8);binary=xshm_read(peer,16,8);
    if(!text || !binary || !xshm_zero(peer) || !xshm_read_int64(peer,1,&value) || value!=0)goto done;
    puts("new data zero: true");
    xshm_close(peer);peer=NULL;
    if(memcmp(text,"A\0你好",8) || text[8] || xrtBufferView(binary).Size!=8 || memcmp(xrtBufferView(binary).Data,"A\0你好",8))goto done;
    puts("snapshot text bytes: 8");
    printf("snapshot binary: ");
    for(size_t i=0;i<8;++i)printf("%02x",((const unsigned char*)xrtBufferView(binary).Data)[i]);
    putchar('\n');result=0;
done:
    xrtBufferDestroy(binary);xrtFree(text);xrtBufferDestroy(wire);xshm_close(peer);
    return result;
}
