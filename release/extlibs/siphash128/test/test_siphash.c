/* Current split: XRT owns 64-bit SipHash; this package owns the 128-bit engine.
 * The removed combined package's xrtsiphasher/reader API is not a dependency. */
#include "xrt.h"
#include "../third_party/siphash.h"
#include "reference/siphash.c"
#include "reference/vectors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64 numeric(const uint8 bytes[8])
{
    uint64 value = 0;
    for (unsigned i=0;i<8;++i) value |= (uint64)bytes[i] << (8*i);
    return value;
}

static void check(const uint8* data, size_t size, const uint8 key[16], bool splits)
{
    uint8 expected64[8], expected128[16], digest[18];
    xsipkey nativeKey = xrtSipKey(numeric(key), numeric(key+8));
    xsiphash narrow={0}, narrowSaved;
    siphash128_state wide={0}, wideSaved;
    assert(siphash(data,size,key,expected64,8)==0);
    assert(siphash(data,size,key,expected128,16)==0);
    assert(xrtSipHash(data,size,nativeKey)==numeric(expected64) && !xrtGetError());
    xrtSipHashInit(&narrow,nativeKey);
    siphash128_init(&wide,key);
    for (size_t position=0;position<size;) {
        size_t count=position%193+1;
        if (count>size-position) count=size-position;
        assert(xrtSipHashUpdate(&narrow,data+position,count));
        siphash128_update(&wide,data+position,count);
        position+=count;
    }
    assert(narrow.Total==size && xrtSipHashUpdate(&narrow,NULL,0));
    memcpy(&narrowSaved,&narrow,sizeof(narrow)); memcpy(&wideSaved,&wide,sizeof(wide));
    assert(xrtSipHashFinal(&narrow)==numeric(expected64));
    memset(digest,0xa5,sizeof(digest));
    siphash128_final(&wide,digest+1);
    assert(!memcmp(digest+1,expected128,16) && digest[0]==0xa5 && digest[17]==0xa5);
    assert(!memcmp(&narrow,&narrowSaved,sizeof(narrow)) && !memcmp(&wide,&wideSaved,sizeof(wide)));
    if (splits) for (size_t split=0;split<=size;++split) {
        xrtSipHashInit(&narrow,nativeKey); siphash128_init(&wide,key);
        assert(xrtSipHashUpdate(&narrow,data,split) && xrtSipHashUpdate(&narrow,data+split,size-split));
        siphash128_update(&wide,data,split); siphash128_update(&wide,data+split,size-split);
        assert(xrtSipHashFinal(&narrow)==numeric(expected64));
        siphash128_final(&wide,digest+1);
        assert(!memcmp(digest+1,expected128,16));
    }
}

int main(void)
{
    static uint8 input[65545];
    static const size_t extra[]={64,65,127,128,129,255,256,257,511,512,513,4095,4096,4097,65535,65536,65537};
    uint8 key[16], digest[16];
    for(size_t i=0;i<sizeof(input);++i) input[i]=(uint8)i;
    for(unsigned i=0;i<16;++i) key[i]=(uint8)i;
    xsipkey official=xrtSipKey(numeric(key),numeric(key+8));
    for(size_t size=0;size<64;++size) {
        siphash128_state state;
        assert(xrtSipHash(input,size,official)==numeric(vectors_sip64[size]));
        siphash128_init(&state,key); siphash128_update(&state,input,size); siphash128_final(&state,digest);
        assert(!memcmp(digest,vectors_sip128[size],16));
    }
    for(unsigned mode=0;mode<4;++mode) {
        for(unsigned i=0;i<16;++i) key[i]=(uint8)(mode ? i*37+mode*19 : i);
        for(unsigned offset=0;offset<8;++offset) {
            for(size_t size=0;size<64;++size) check(input+offset,size,key,true);
            for(size_t i=0;i<sizeof(extra)/sizeof(*extra);++i) check(input+offset,extra[i],key,false);
        }
    }
    xsiphash state={0}, saved;
    xrtSipHashInit(&state,official); memcpy(&saved,&state,sizeof(state));
    assert(!xrtSipHashUpdate(&state,NULL,1) && xrtGetError());
    assert(!memcmp(&state,&saved,sizeof(state))); xrtClearError();
    assert(!xrtSipHashUpdate(&state,&state,1) && xrtGetError());
    assert(!memcmp(&state,&saved,sizeof(state))); xrtClearError();
    state.Total=UINT64_MAX; state.TailSize=7; memcpy(&saved,&state,sizeof(state));
    assert(!xrtSipHashUpdate(&state,input,1) && xrtGetError());
    assert(!memcmp(&state,&saved,sizeof(state))); xrtClearError();
    state.Guard=0; memcpy(&saved,&state,sizeof(state));
    assert(!xrtSipHashUpdate(&state,input,1) && xrtGetError());
    assert(!memcmp(&state,&saved,sizeof(state))); xrtClearError();
    puts("SipHash current split: 128 official answers; 4 keys x 8 alignments x 81 lengths; splits, nonmutating finalization, overlap/state/overflow refusals passed");
    return 0;
}
