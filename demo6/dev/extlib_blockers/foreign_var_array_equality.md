# 原生 xvalue 数组返回后的内容相等性不一致

2026-10-24，使用当前 Windows 发布版 `release/xl.exe`，从 `demo6` 运行：

```powershell
release/xl.exe check dev/extlib_blockers/money_split_array_equality.xl
release/xl.exe run dev/extlib_blockers/money_split_array_equality.xl
```

`check` 输出 `ok`。JIT 中普通 `var` 数组、`array<int>` 与相同内容
字面量的 `==` 都为 `true`；`money.split(101, 3)` 返回的原生 XRT
`xvalue` 数组长度和三个元素分别为 `3, 34, 34, 33`，但它与
`[34, 34, 33]` 比较为 `false`，随后断言失败、退出码 1。
同一复现在独立 EXE 中先观察到完全相同的三行比较输出；
最终断言版应在主线修复时同时复验 JIT 与 AOT。

`money_native.c::xm_split` 用 `xrtValueArray` 和 `xrtValueInt` 构造结果，
元素值本身正确；差异只发生在 foreign `var` 返回容器与语言容器的
相等性边界。建议主线审查外来 `XVALUE_ARRAY` 的比较分派、元素
投影及运行时类型身份，使同内容的原生/语言数组遵守一致的 `==`
合同，并补 `!=`、嵌套数组及 EXE 回归。不要在 money 包内把结果逐项
复制成另一种数组来掩盖此问题。

`release/extlibs/money/test/check.ps1` 的 C 测试、语言主用例和错误类型
测试先通过，在 `test/allocations.xl` 的故障后恢复断言处失败；该断言
使用 `money.split(101, 3) == [34, 34, 33]`。包的完整 JIT/AOT 门禁
尚未签收。修复相等性后，需重跑最小 JIT/AOT 与默认包级门禁；若分配
故障扫描的 24 点上限仍未覆盖首个成功点，再单独调整扫描窗口。
