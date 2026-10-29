# NUL 检查的常量针串分配

## 定性与范围

2026-10-28 的非阻断性能观察，不是功能 BUG，不增加发布矩阵的 M/R。
Diff/Fuzzy 的两个输入、Geohash 的 hash 输入需要拒绝内嵌 NUL，现有
`string.indexOf(text, "\0", false, string.byteLen(text), 1)` 语义正确，
但构造小型针串仍会分配。不能删除检查，或为省分配把错误文本截断。

尝试把针串移到全局 `const string`，当前 Windows 编译器仍在读取
常量作实参时生成正文复制，没有改善该分配。没有向产品引入缓存、
额外 C 适配层，也没有改编译器或 XRT。

## 复现与证据

从 `demo6` 目录运行同名 `.xl`：

```powershell
release\xl.exe run dev\extlib_blockers\cstring_nul_constant_allocation.xl
release\xl.exe cgen dev\extlib_blockers\cstring_nul_constant_allocation.xl --emit build\test\cstring_nul_constant_allocation.c
```

当前 Windows 发布物 SHA256：
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
源文本与全局初始化在故障注入之前；只允许零次后续 XRT 分配时，
输出 `constant needle triggered: true`、`constant needle memory: true`，
退出 0。生成 C 的全局初始化把单字节 NUL 放入字符串，读取此全局
的调用链随后执行 `xlir_string_copy`。本记录仅确认该 Windows
JIT/cgen 路径，未据此宣称 Linux 或其他版本也有相同成本。

这不是吞吐基准，也不表示扫描正文没有成本；故障注入只证明至少
存在一次需要成功分配的路径。Diff 的独立预算回归另明确记录语言
相对 C 的两次小型针串分配，不能借此称整个桥接零开销。

## 主线改进方向

- 在生命周期/逃逸分析能够证明安全时，让不可变字面量或常量以借用
  视图传递；常量释放、动态模块租约与闭包逃逸应由编译器统一处理。
- 或将有界 NUL 检查编译为现有 XRT span 扫描/CRT `memchr` 的直接
  调用，在明确长度内查询，不创建针串，不扫描到字符串边界外。
- 此工作交主线上下文统一决定；不在每个扩展包复制一份适配器，
  也不伪造与真实 `const void*` ABI 冲突的 extern 声明。

现有扩展包继续保留正确的检查、MemoryError 和输入错误合同。
