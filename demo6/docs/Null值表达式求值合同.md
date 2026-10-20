# Null 值表达式求值合同

## 目标与边界

`null == null` 的真值可静态确定，但不能因此省略产生逻辑空值的表达式。
`await future<null>` 没有物理载荷，却仍有等待、取消、错误和拥有清理的效果。
此次在共用 XLIR 表达式层修复比较与 `type(...)`，不为 Promise、Channel
或 Cgen 增设特判，也不增加运行时实现。

这不新增普通函数的 null 形参／返回值 ABI、null 存储、捕获或合并能力。
原生 `generic-zero-payload` 合同保持独立；见 [零负载参数合同](XLIR零负载参数合同.md)。

## 表示与求值

`xl6IrNullOperandPlanBuild` 从精确类型和生产者结构给出三种事实：普通物理值、
无效果的 null 字面量、无载荷的 checked await。空值身份来自 runtime type map，
不是类型名称、指针宽度或调用目标名称。无效计划不发布部分输出。
`xl6IrNullComparisonPlanBuild` 进一步检查二元操作和 bool 结果合同。

- 两侧都属逻辑空值时，按源码从左到右完成效果，成功后才产生 EQ／NE 的常量。
- 一侧是物理可空值时，仍按源码顺序取得它的原始存储表示，再进行 sentinel 测试；
  不先按 flow narrowing 消掉 null，也不在右侧完成后重新读取左侧变量。
- 左侧的拥有值在右侧执行或挂起前保护，沿用普通表达式完成／回滚清理。
  外部对原变量的替换不能让该快照提前析构。
- `type(await f)` 先完成 await 的效果，再产生已知类型身份；原 TypeId 比较规则不变。
- 左侧失败不求值右侧。取消仍为独立状态，原始 await 错误优先于清理错误，
  无原始失败时清理错误仍须 checked 报告；`finally` 行为不变。

普通 await 和上述效果求值复用同一个 lowering 实现。它将“完成成功”与
“存在返回载荷”分开，不能把 `XL6_IR_VALUE_INVALID` 同时当作正常 void
和失败。VOID AWAIT 的结果只作 ERROR_TEST 依赖 token，不实体化为 C 值，
不进行复制、取地址、存储或跨挂起保存。

## 验证入口

`tools/check_xlir_null_value_comparison.py --source-build <build.json> --out <fresh-directory>`
要求完整当前源码编译器，验证输入／对象、日志及产物哈希：

- 三份真实语言用例：严格 C11 O0／O2、JIT、模块执行和独立 AOT；AOT 不选择 XRT ALL。
- 真实 pending Promise／完成屏障证明左右顺序与取消，不靠已完成 Future 冒充挂起。
- 可空类的左值快照跨真实挂起；精确一次析构、成功后的清理失败及原始失败优先。
- 24 份既有 Promise、非空 await、泛型、union、Channel、TaskPool 结果及取消回归。
- 两份普通 null 调用 ABI 的拒绝用例，拒绝后无 C 文件。
- O0／O2 C 合同各检查 13 个比较、19 个空值 await、89 个计划查询，
  拒绝 57 个结果／错误元数据变体及 114 次 C/header 输出；输出必须为零字节。

此门不替代发布 DLL／SO、完整 P1 或五组联合验收，也没有网络或安全性质测试。

## 扩大回归发现的旧基线

发现轮次另测了 `xlir_fixed_executor_language` 和 `stdlib_task_pool_exit_wait`，
不能算通过。前者 Windows 业务断言完成后仍在模块退役中反复收到原生 TaskPool
BUSY，当前编译器到达 240 秒上限；修改前完整源码编译器同样保持 BUSY，
其工作线程已退出。后者 Linux 在物理退出成功后，重复零超时／取消等待未按
“完成优先”返回；修改前完整源码也复现相同断言失败，但其他执行可以通过，
因此必须按时序相关问题处理，不能将一次通过视为修复。

两项仍属原长期任务的运行时退役／等待工作，不因本次表达式修复而关闭。
限定门不包含这两项；失败的扩大轮次保留为发现证据，不与之后限定验收混用。

## 本批限定验收

2026-10-20，完整当前源码构建为 Windows 624／Linux 623 单元，最终两端
限定报告各 78 阶段，共 156 阶段通过（每端两项预期拒绝）。三份用例的
JIT／模块／独立 AOT、严格 C 和两种优化的 IR 合同均通过；输入、日志及
产物哈希一致。证据位于
`F:/xlang-validation/five-groups-20261020/p0-null-value-comparison/`：
`source-*-r1` 和 `language-*-r2` 是本批限定验收，`language-*-r1` 是未通过的
扩大回归发现，`baseline-observations.json` 保留旧编译器对照与后续可通过的
重复窗口，不据此关闭时序问题。

### 后续运行时收口

上述两个旧基线随后在驻留 TaskPool 收尾与完成合同中修复，并补齐独立 AOT 的
Commit 后释放边界，见 [TaskPool 收尾完成合同](TaskPool收尾完成合同.md)。新批次
单独验收；不回写本页原失败发现记录，也不扩大原 null 专项报告的证明范围。
