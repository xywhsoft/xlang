/* 仅用于生成独立答案；链接 README 指定的上游参考实现，不链接本包算法。 */
#include "MurmurHash3.h"
#include <cstdio>
#include <cstring>

static void vector(const unsigned char* data, unsigned size, uint32_t seed, const char* id)
{
    uint32_t h32;
    uint64_t h128[2];
    unsigned char* bytes = reinterpret_cast<unsigned char*>(h128);
    MurmurHash3_x86_32(data, static_cast<int>(size), seed, &h32);
    MurmurHash3_x64_128(data, static_cast<int>(size), seed, h128);
    std::printf("{\"size\":%u,\"seed\":%u,\"h32\":%u,\"hex128\":\"", size, seed, h32);
    for (unsigned i = 0; i < 16; ++i) std::printf("%02x", bytes[i]);
    std::printf("\",\"id\":\"%s\"}", id);
}
int main()
{
    /* 上游装载使用对齐整数读取，参考答案生成只在小端平台运行。 */
    const uint32_t endian = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian) != 1) return 1;
    alignas(16) static unsigned char data[65537];
    const uint32_t seeds[] = {0, 1, 42, UINT32_MAX};
    const unsigned edges[] = {255,256,257,511,512,513,1023,1024,1025,4095,4096,4097,65535,65536,65537};
    for (unsigned i = 0; i < sizeof(data); ++i) data[i] = static_cast<unsigned char>(i * 37 + 11);
    std::puts("{\"pattern\":\"byte[i]=(i*37+11)%256\",\"vectors\":[");
    bool first = true;
    for (unsigned seed : seeds) {
        for (unsigned i = 0; i < 144; ++i) {
            if (!first) std::puts(",");
            first = false;
            vector(data, i <= 128 ? i : edges[i - 129], seed, "pattern");
        }
    }
    std::puts("],\"fixed\":[");
    const char* texts[] = {"", "abc", "foo", "\xe4\xbd\xa0\xe5\xa5\xbd\xf0\x9f\x8c\x8d\xe5\xb0\xbe", "123456789"};
    const char* ids[] = {"empty", "abc", "foo", "utf8", "digits"};
    first = true;
    for (unsigned seed : seeds) {
        for (unsigned i = 0; i < 5; ++i) {
            if (!first) std::puts(",");
            first = false;
            /* 复制到对齐工作区，不让参考实现的未对齐假设影响生成。 */
            unsigned size = static_cast<unsigned>(std::strlen(texts[i]));
            std::memcpy(data, texts[i], size);
            vector(data, size, seed, ids[i]);
        }
    }
    std::puts("]}");
    return 0;
}
