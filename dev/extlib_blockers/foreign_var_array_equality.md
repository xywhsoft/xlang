> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# 原生 xvalue 数组返回后的内容相等性不一致

2026-10-30 根目录布局复验：两平台默认 money 门禁均完成 51 项可运行
前置检查（含新算术独立参照、九条生成原型、新旧双语与十条独立程序），
第 52 项原分配正例仍在数组相等性断言退出 1。最小源从仓库根执行
`release/xl.exe run dev/extlib_blockers/money_split_array_equality.xl`
（Linux 使用 `release/xl`），JIT/独立程序仍显示 plain/typed 为 true、
split 为 false，元素正确；check/build 仍成功。C 范例 AOT 这次已移到
阻断前并实际通过，但不能代替原分配/相等性边界。没有改相等断言或
逐项重建数组；修复建议仍为主线共享比较分派、投影与所有权处理。
当前具体报告见验收矩阵和任务台账 2026-10-30 Money 条目。下文为历史
执行顺序，不能用旧“没有执行到 C 范例”描述当前门禁。

2026-10-26 重建发布版复验：`money_split_array_equality.xl` 在
Windows/Linux 均输出 `plain: true`、`typed: true`、`split: false`，
且拆分结果长度与三个元素仍为 `3, 34, 34, 33`，最后断言退出 1。
一次将包内分配恢复测试临时改为逐元素比较后两平台门禁可通过，
但这绕开了外来数组的公开相等性合同，不能作为修复或签收证据；
包内红色相等断言已恢复。后续新增的 C 范例 AOT 检查在主线修复前
不会由默认门禁执行到。修复后须重新跑最小源、分配故障和完整双平台
门禁，再确认 C/XLang 范例独立 EXE 输出一致。

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
