/* 记录真实编码、边界与四邻格；Python 用精确有理数网格独立验证。 */
#define XRT_MODULE_STRING
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/geohash_native.h"
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char alphabet[] = "0123456789bcdefghjkmnpqrstuvwxyz";
static unsigned count, failures;
static bool records;
#define CHECK(c) do { if (!(c)) { if (failures < 8) { \
    fprintf(stderr,"geohash grid line %d: %s\n",__LINE__,#c); } ++failures; } } while (0)

static double number(uint64_t bits)
{
    double value;
    memcpy(&value,&bits,sizeof(value));
    return value;
}

static void print_bits(double value)
{
    uint64_t bits;
    memcpy(&bits,&value,sizeof(bits));
    printf("%016llx",(unsigned long long)bits);
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    CHECK(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount);
    CHECK(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static void encode_pair(double lat, double lon)
{
    for (int precision=1; precision<=12; ++precision) {
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        char* hash=xgeohash_encode(lat,lon,precision);
        CHECK(hash && !xrtGetError() && strlen(hash)==(size_t)precision);
        if (records) {
            printf("E\t%u\t%d\t",count,precision); print_bits(lat); putchar('\t'); print_bits(lon);
            printf("\t%s\n",hash ? hash : "<error>");
        }
        xrtFree(hash); balanced(&before); ++count;
    }
}

static void cell(const char* hash)
{
    xgeohash_bounds bounds={0};
    char* neighbors[4]={0};
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    CHECK(xgeohash_decode_bounds(hash,&bounds) && !xrtGetError());
    double lat=xgeohash_decode_lat(hash); CHECK(!xrtGetError());
    double lon=xgeohash_decode_lon(hash); CHECK(!xrtGetError());
    int precision=xgeohash_precision(hash); CHECK(precision>0 && !xrtGetError());
    char* bbox=xgeohash_bbox(hash); CHECK(bbox && !xrtGetError());
    for (int d=0; d<4; ++d) {
        neighbors[d]=xgeohash_neighbor(hash,d);
        CHECK(neighbors[d] ? !xrtGetError() : xrtErrorKind(xrtGetError())==XERR_RANGE);
    }
    if (records) {
        printf("C\t%u\t%s\t%d\t",count,hash,precision);
        double values[]={bounds.min_lat,bounds.min_lon,bounds.max_lat,bounds.max_lon,lat,lon};
        for (int i=0; i<6; ++i) {print_bits(values[i]); putchar('\t');}
        printf("%s",bbox ? bbox : "<error>");
        for (int d=0; d<4; ++d) printf("\t%s",neighbors[d] ? neighbors[d] : "-");
        putchar('\n');
    }
    for (int d=0; d<4; ++d) xrtFree(neighbors[d]);
    xrtFree(bbox); balanced(&before); ++count;
}

static uint32_t next(uint32_t* state)
{
    *state=*state*UINT32_C(1664525)+UINT32_C(1013904223);
    return *state;
}

int main(int argc, char** argv)
{
    records=argc==2 && strcmp(argv[1],"--records")==0;
    if (argc!=1 && !records) return 2;
    if (sizeof(double)!=sizeof(uint64_t) || DBL_MANT_DIG!=53 || FLT_RADIX!=2) return 2;
    CHECK(xrtMemDebugEnabled());
    xrtSetErrorInfo(XERR_STATE,"test.geohash",0,"warm error slot"); xrtClearError();
    /* 零的两侧、闭端点内侧、45/90 度分割线两侧均保留真实 binary64 位。 */
    static const uint64_t pairs[][2]={
        {0,0}, {UINT64_C(0x8000000000000000),0}, {1,UINT64_C(0x8000000000000001)},
        {UINT64_C(0x8000000000000001),1},
        {UINT64_C(0x4056800000000000),UINT64_C(0x4066800000000000)},
        {UINT64_C(0xc056800000000000),UINT64_C(0xc066800000000000)},
        {UINT64_C(0x4056800000000000),UINT64_C(0xc066800000000000)},
        {UINT64_C(0xc056800000000000),UINT64_C(0x4066800000000000)},
        {UINT64_C(0x40567fffffffffff),UINT64_C(0x40667fffffffffff)},
        {UINT64_C(0xc0567fffffffffff),UINT64_C(0xc0667fffffffffff)},
        {UINT64_C(0x4046800000000000),UINT64_C(0x4056800000000000)},
        {UINT64_C(0x40467fffffffffff),UINT64_C(0x40567fffffffffff)},
        {UINT64_C(0x4046800000000001),UINT64_C(0x4056800000000001)},
        {UINT64_C(0xc046800000000000),UINT64_C(0xc056800000000000)},
        {UINT64_C(0xc046800000000001),UINT64_C(0xc056800000000001)},
        {UINT64_C(0xc0467fffffffffff),UINT64_C(0xc0567fffffffffff)}
    };
    for (unsigned i=0; i<16; ++i) encode_pair(number(pairs[i][0]),number(pairs[i][1]));
    uint32_t state=UINT32_C(0x6927c31d);
    for (unsigned i=0; i<64; ++i) {
        double lat=((int)(next(&state)&65535)-32768)*(90.0/32768.0);
        double lon=((int)(next(&state)&65535)-32768)*(180.0/32768.0);
        encode_pair(lat,lon);
    }
    char hash[13];
    for (int precision=1; precision<=2; ++precision) {
        for (unsigned index=0; index<(1u<<(5*precision)); ++index) {
            for (int at=0; at<precision; ++at) hash[at]=alphabet[(index>>(5*(precision-at-1)))&31];
            hash[precision]=0; cell(hash);
        }
    }
    static const char* corners[]={"000000000000","zzzzzzzzzzzz","bpbpbpbpbpbp","pbpbpbpbpbpb"};
    state=UINT32_C(0x1a2b3c4d);
    for (int precision=3; precision<=12; ++precision) {
        for (unsigned i=0; i<68; ++i) {
            for (int at=0; at<precision; ++at) hash[at]=i<4 ? corners[i][at] : alphabet[next(&state)>>27];
            hash[precision]=0; cell(hash);
        }
    }
    CHECK(count==2696);
    if (failures) {fprintf(stderr,"geohash grid: %u failed checks\n",failures); return 1;}
    if (!records) puts("geohash grid: 2696 records, PASS");
    return ferror(stdout) ? 1 : 0;
}

#include "../geohash_native.c"
