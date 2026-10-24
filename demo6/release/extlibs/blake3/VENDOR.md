# BLAKE3 来源与构建

## 固定来源

- 项目：[BLAKE3-team/BLAKE3](https://github.com/BLAKE3-team/BLAKE3)。
- Tag：`1.5.0`。
- Commit：`5aa53f07f7188a569cadfc5daf1522972d9a9630`。
- 许可证：上游提供 CC0 1.0 或 Apache License 2.0 两种选择，完整文本随包保留于
  [third_party/LICENSE](third_party/LICENSE)。
- 每个文件的上游路径、原始 SHA-256 和本地 SHA-256 位于
  [third_party/manifest.json](third_party/manifest.json)。构建不下载或自动升级上游。

`third_party` 包含上游 `c/blake3.c`、`c/blake3.h`、`c/blake3_impl.h`、
`c/blake3_portable.c`。独立已知答案来自同一 commit 的
`test_vectors/test_vectors.json`，放入 [test/test_vectors.json](test/test_vectors.json)。
其中 35 组向量同时记录 hash、keyed_hash、derive_key 的 131 字节输出。
测试数据是从 0 开始的循环字节序列，字节 `i` 为 `i % 251`，不是全零数据。

## 本地差异

`third_party/blake3_impl.h` 与固定上游版本相比，仅增加空行并删除三处行尾空格；
非空行的实际内容一致。清单分别记录上游原件和本地文件的 SHA-256，不能互换。

`third_party/blake3.c` 中 `compress_chunks_parallel` 的局部数组改为零初始化：

```c
const uint8_t *chunks_array[MAX_SIMD_DEGREE] = {0};
```

这是为了便携单路构建下 GCC `-O2 -Werror` 的确定性初始化检查，不改变实际填充的
输入指针、压缩步骤或哈希算法。上游原始字节与本地版本各有哈希，不将修改后的文件
冒充上游原样文件。

## 包装层和构建边界

[blake3_portable.c](blake3_portable.c) 是项目自己的小型构建适配：

1. 三个压缩入口直接绑定 portable 实现。
2. `MAX_SIMD_DEGREE=1`，`MAX_SIMD_DEGREE_OR_2=2`，NEON 关闭。
3. 编译上游核心和 portable 实现，`blake3_simd_degree()` 固定返回 1。

不编译 CPU 探测、SSE/AVX/NEON 汇编或多线程任务调度。这避免无关指令集依赖和
全局 CPU 探测状态，但不代表达到上游 SIMD 后端性能。不要同时将适配文件及其已包含
的上游 `.c` 再次加入构建，包清单只编译适配、XRT 核心接口及语言薄绑定三个单元。

`xrt_blake3.c` 与 `blake3_native.c` 是项目接口层，不属于上游补丁。
核心测试为计数溢出白盒检查直接包含 `xrt_blake3.c`，此测试不能再链接该核心目标文件；
原生/分配测试及普通应用则通过公开头链接核心。

## 独立向量与更新方式

[test/vectors.h](test/vectors.h) 是 JSON 向量的机械投影。Windows 门禁逐条对照
长度、三个输出、密钥和上下文，并检查全部六份上游锁定文件。
双语范例黄金输出从相同独立向量截取，不从被测实现生成预期值。

`.gitattributes` 将锁定文件、向量头、黄金输出与 Linux 脚本固定为 LF，避免 checkout
自动换行破坏跨平台哈希。更新上游时须人工审阅源码和许可证、重新评估上述补丁、
更新锁定清单及向量投影，然后重新执行 Windows/Linux 专项及 C/xlang 范例。
测试通过不能替代来源审查或密码学实现审计。
