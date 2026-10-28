# 动态整数的显式 float 转换误走 checked as

状态：blocked-upstream（M10）。普通扩展库任务不修改 XLang 主线，也不在
JSON5 内添加转换包装、私有值读取或改成字符串中转来绕过。

## 预期与复现

[正式类型转换规则](../../docs/类型转换规则.md)明确区分：`float(x)` 是显式
数字转换，`as` 才是精确类型取出/受检查转换。动态显式标量转换应复用 XRT；
`var` 中的整数 42 显式转为 float 应成功，结果为 42.0。

在 `demo6` 运行不导入任何包的最小源：

```powershell
./release/xl.exe check dev/extlib_blockers/var_int_explicit_float.xl
./release/xl.exe run dev/extlib_blockers/var_int_explicit_float.xl
```

`check` 返回 0/ok，`run` 返回 1：

```text
uncaught xlang error: TypeError: dynamic value does not match requested as target
uncaught xlang error: dynamic value does not match requested as target
```

JSON5 的真实消费用例为
[`test/numeric_consumer.xl`](../../release/extlibs/json5/test/numeric_consumer.xl)：
`getValue` 返回 int 类型的 `var` 后，`float(value)` 同样失败；解析字典字段
也应按同一规则转换。该用例保持正确的正例断言，已纳入默认门禁和包覆盖清单；
不能把抛 TypeError 改成测试的预期结果。

## 当前定位

本工作树编译器源码基线为 `d64672903`，不是实时更新的 master。
Windows 发布物指纹见[验收矩阵](../../docs/普通扩展库发布验收矩阵_2026-10-28.md)。
Windows 和 Linux 的无包最小源均 `check` 成功、JIT 抛同一 TypeError；
两平台独立 AOT 构建成功，执行仍抛该错误。JSON5 的两平台默认门禁均先
通过 C、边界、分配、既有双语 JIT/AOT 检查，再在新消费正例返回 1。
Linux 在 `/var/tmp` 使用经目录差分和 SHA-256 核对的同输入原生文件系统
快照，C 带 ASan/UBSan；不能把这一结果外推为挂载盘的 AOT 耗时风险已关闭。

对最小源显式 `xl cgen` 的实际输出为：

```c
v4 = xrtValueInt(42LL);
s4 = v4;
v5 = s4;
(void)xlir_xvalue_cast_checked(v5, 0x83EE8F0746BF37D7ULL, 4, &v6);
```

同一生成文件中的 `xlir_xvalue_cast_checked` 先要求源的实际 type id 等于
目标 type id，再取 double。整数自然在调用数值转换前被拒绝。静态字符串
`float("42")` 则成功，说明这不是 JSON5 数字解析器错误。

相关主线位置（本基线行号）：

- `src/ir/xl6_ir_lower.c:7934`：checked xvalue cast runtime op 选择。
- `src/cgen/xl6_ir_cgen_value_projection.c:551`、`:605`：checked cast 发射。
- `src/cgen/xl6_ir_cgen_emit_xvalue_core.c:161`、`:200`：精确类型判断及 TypeError。

生成 C 足以确认错误路径，但尚未在本任务中逐层证明语义阶段为何为显式构造
选择了它；修复不应只删除运行时的 type id 检查，因为普通 `as`、参数和
容器准入仍依赖严格检查。

## 主线修复与验收建议

1. 在 typed HIR/IR 中明确区分显式标量构造与 checked extraction，前者调用
   现有 XRT 数值转换，保留类型、范围、字符串长度、错误与 OOM 合同。
2. 核对 bool/int/uint/float/string 动态源的合法转换及范围/非法文本/内嵌 NUL
   负例；非标量不能因这次修复被隐式当作数字文本。
3. 保留 `as`、typed callable/容器读取的精确检查，不用宽松 getter 放宽它们。
4. 分别验证纯语言最小源和 JSON5 原生返回值/字典字段的 JIT 与独立 AOT；
   再运行 JSON5 默认 Windows/Linux 门禁，不以 SkipAot 或包内绕过签收。

暂只把直接验证的 JSON5 记为受影响包，不推断其他 99 包已经同样失败。
