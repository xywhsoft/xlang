# M14：Windows 嵌入 C 的 snprintf 截断不符合 C99

## 范围与复现

2026-11-02，在现行 Windows 发布版复现。最小源
`dev/extlib_blockers/c99_snprintf_termination.c` 不依赖 XRT 或任何扩展包。
它只使用有界字节比较及 `memchr`，不会把未终止数组当 C 字符串读取。

```powershell
gcc -std=c11 -O2 dev/extlib_blockers/c99_snprintf_termination.c -o build/snprintf-gcc.exe
./build/snprintf-gcc.exe
./release/xl.exe run dev/extlib_blockers/c99_snprintf_termination.c
./release/xl.exe build dev/extlib_blockers/c99_snprintf_termination.c -o build/snprintf-aot.exe
./build/snprintf-aot.exe
```

GCC 输出两行，退出0：

```text
snprintf: result=6 zero=3 guards=1 bytes=61626300
vsnprintf: result=6 zero=3 guards=1 bytes=61626300
```

Windows `xl run` 及生成的 EXE 退出1，输出：

```text
snprintf: result=-1 zero=-1 guards=1 bytes=61626364
vsnprintf: result=6 zero=3 guards=1 bytes=61626300
```

这里确认出错的是 `snprintf`；`vsnprintf` 对照符合本例预期，不将其误报为
两者都坏。实际 DLL 路径及其他平台须分别复验，不能从 Windows CLI 外推。

同日在冻结 Linux 原生目录复验：GCC 15.2、`xl run` 和独立 EXE 均输出
两行标准结果并退出0；三包 Reader 的完整长消息快照亦在 GCC ASan/UBSan、
TCC 内存运行和AOT下通过。当前已证实的是 Windows 两条 CLI 用户 C 路径，
不将 Linux 或尚未测试的 DLL 宿主记成同一故障。

## 影响及建议

xxhash、BLAKE2、BLAKE3 的可选 C 错误快照对长原文使用标准 `snprintf`。
Windows TCC 路径会把256字节写满而无终止符；本轮有界合同每算法检出108项
长快照失败。原始 XRT 错误仍在，不能据此宣称错误快照安全或整包发布通过。

这更像旧 CRT `_snprintf` 语义进入了用户 C 环境，但具体头文件/符号绑定点
尚未确定，不能只根据函数名作根因结论。主线应统一内存运行、AOT 及 DLL 的
C99 格式化声明/绑定：正容量截断时写终止符、返回完整所需长度；优先复用正确
的宿主 CRT 接口，不让每个扩展写私有补丁。应补容量0/1、刚好容纳、长文本、
可变参数及守卫字节测试，并复验真实错误快照消费者。

本任务不修改 TCC/CRT 注入或重建并行修改中的主线；包门禁继续要求真实退出0，
不把这个失败改成预期成功，也不通过额外补零隐藏共享缺陷。
