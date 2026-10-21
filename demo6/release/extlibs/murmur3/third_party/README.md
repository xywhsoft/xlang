# MurmurHash3 来源

算法来自 Austin Appleby 的 SMHasher：
[固定参考提交](https://github.com/aappleby/smhasher/tree/07bb4de10a63e8cc2e1724865454eba635742383)。
上游把 MurmurHash3 置于 public domain；本包的 C 文件是本地移植，不冒充未改动的上游文件。

保留 x86_32 与 x64_128 算法。主要本地改动：显式小端 load/store、流式状态、
不可变 digest 快照、直接整块处理、部分尾块缓存、空输入处理；一次性接口复用
流式核心，避免两份算法漂移。资源预算、Reader、错误与回调由上层 XRT 风格核心承担。

## 独立参考答案

测试答案链接原始 C++ 实现，不链接本包 C 算法。文件记录在 `test/vectors.json`
的 oracle 字段；普通门禁只使用已落盘 fixture，不联网下载代码。

| 文件 | SHA-256 |
| --- | --- |
| 上游 src/MurmurHash3.cpp | 30f121ed155ebf336af398aabb7d8d157afdfafc8d981e7b48d2a1ceb4b63e4e |
| 上游 src/MurmurHash3.h | 9af2003e3885c841fa0ac08cb5d5f31541ee9d793f8a9c4a4f771f43fb0d6f41 |
| test/generate_vectors.cpp | 6d1945da6dade3ecd3f22bffa26c8203cfd9c1ffbe3e9c2ee4dadd52670f25ea |

将该固定提交的两个上游文件放到独立工作目录，核对哈希后，在小端机器编译：

```sh
g++ -std=c++11 -O2 -fno-strict-aliasing -Ibuild/test/murmur3_oracle \
    release/extlibs/murmur3/test/generate_vectors.cpp \
    build/test/murmur3_oracle/MurmurHash3.cpp \
    -o build/test/murmur3_oracle/generate
build/test/murmur3_oracle/generate
```

输出 JSON 含 576 个 pattern 答案及 20 个固定文本答案。pattern 为
`byte[i]=(i*37+11)%256`，seed 为 0、1、42、UINT32_MAX；长度为 0..128 和
255/256/257、511/512/513、1023/1024/1025、4095/4096/4097、65535/65536/65537。
原始 C++ 实现按本机整数装载，生成器使用对齐缓冲并拒绝大端；本地 C 核心另外
在 0..7 地址偏移执行每组答案。fixture 的 `test/vectors.h` 由 JSON 的 vectors
数组机械生成。测试不是一个实现与自身输出对照，也不代表实际大端/32 位机器验收。
