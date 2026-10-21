# XZ 编解码源码

本目录按需引入 [7-Zip 26.03](https://github.com/ip7z/7zip/tree/26.03/C) 的公共领域 C 源码；
`xlang.package.json` 只编译 XZ/LZMA2 所需的 19 个实现文件，不引入外部运行库。

本包维护四处本地补丁：

- `Precomp.h` 定义 `Z7_ST`，只构建单线程编解码器。
- `CpuArch.c` 在 TCC 下引入 `tcc_stub.h`，明确禁用无法检测的硬件加速。
- `CpuArch.h` 使用定义良好的逐字节未对齐读写，避免 BCJ 解码器在合法 `.xz` 流上触发 C 未定义行为。
- `LzmaEnc.c` 逐字节写入可能未对齐的 32 位属性，避免未定义行为。

升级上游版本时须保留这四处补丁，并复跑 Windows TCC、Linux ASan/UBSan 和 Python XZ 双向互通测试。
