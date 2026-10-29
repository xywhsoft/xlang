# C unsigned long 夹具可移植性

`c_abi_types.xl` 原来把 `c_ulong` 直接传给有符号 64 位 `int`。Windows 当前
工具链的 `unsigned long` 为 32 位，可以无损隐式扩宽；Linux 为 64 位，不能
隐式转换为 `int`。实际编译器宏分别报告 `__SIZEOF_LONG__` 为 4／8。

该断言现在与相邻的 `c_ulonglong`、`c_size` 一样使用显式 `int(...)` 转换。
没有改变语言的溢出、符号或隐式转换规则。

两端固定源编译器均完成 C 输出、实际 JIT、实际 AOT 构建和执行，保留全部
原生 C ABI 回显断言。未修改的旧夹具作为控制：Windows 仍可输出，Linux
仍以 `target int, value c_ulong` 拒绝且不交付 C 文件。

原始 536 项冻结语料和失败报告保持原样。此修复只更新当前测试夹具，不表示
达到 514 项门槛或完成发布验收。验证报告为两端的
`portable-c-ulong-r101/results.json`，独立核对见
`E:/xlang-validation/five-groups-20261027/p7-shipping/portable-c-ulong-proof-r102/results.json`。
