# 嵌套类型化数组 append 运行时报类型错误

2026-10-26 重建发布版复验：`tools/check_extlib_csv.ps1` 先通过
严格 C、分配/文件事务/线程与 454 项 Python 互通，然后中央 XLang JIT
在同一嵌套数组写入处报 `TypeError: value has an incompatible type`。
独立 `release/xl.exe run dev/extlib_blockers/csv_nested_array_value.xl`
仍可复现；公开 `release/examples/extlibs/csv.xl` 正常，说明失败面
在类型化嵌套元素的写入路径，不在一般 CSV 解析或文本输出。

2026-10-24 Windows 发布版最小复现：

```powershell
cd D:\GIT\x-lang\demo6
.\release\xl.exe check dev/extlib_blockers/csv_nested_array_value.xl
.\release\xl.exe run dev/extlib_blockers/csv_nested_array_value.xl
.\release\xl.exe build dev/extlib_blockers/csv_nested_array_value.xl -o build/test/csv_nested_array_value.exe
.\build\test\csv_nested_array_value.exe
```

`check` 输出 `ok`，`build` 成功；JIT 和 EXE 在 `rows.append(array["name", "value"])`
报 `uncaught xlang error: TypeError: value has an incompatible type`，退出码均为 1。
复现源不导入扩展库。`csv` 的综合门禁先通过七组 C 用例（含线程、
分配故障与文件事务）和 454 项 Python 独立互通，随后在中央 XLang
用例第一次执行同形态的嵌套数组写入时失败；因此不能将其归咎于
CSV 解析器或以包内转换层规避。

建议在主线审查类型化容器的嵌套元素构造、插入与运行时类型身份，
明确 `array<array<T>>` 的元素所有权和投影规则。修复后先运行本最小
文件的 JIT/EXE，再跑 `tools/check_extlib_csv.ps1` 默认门禁，并检查
其他接受嵌套类型化容器的调用路径。本普通扩展库任务不修改主线。
