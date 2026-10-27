# 可调用值的完整容器返回类型

本批修复 `cgen_typed_container_for_in_returned.xl:244` 的真实语言缺口：
`array<int> values = maker.values()` 在早期推导中得到裸 `array`，
而字段保存的函数签名仍返回 `array<int>`。同一表达式用于后续赋值时
却可能正确，说明问题发生在初始化前的类型查询，不能由目的变量类型、
lowering 或 Cgen 补齐。

## 实现与边界

`xl6SemCallReturnTypesEx` 继续优先解析真实函数／方法声明、重载和协议。
找不到声明调用时，对成员及其他表达式形式的可调用值查询完整函数类型，
解析函数别名后，复制其完整返回列表。该查询发生在最终 call plan 选择前，
不执行 getter，不消费调用参数，也不改写调用种类或拥有权。

返回列表归查询者所有，alias 的声明和缓存类型仍为借入对象。单结果保留
泛型参数及 nullable；多结果保留完整的 tuple 组成。生产代码只增加 25 行
语义查询代码，未修改 IR／verifier／Cgen／XRT。没有新增目标程序调用、
分配、复制或同步；新增的类型克隆只发生在编译期。

## 双平台验证

Windows、Linux 各 34 项全部通过：

- 用旧 source-driver 重新证明原夹具确实在精确 closure ABI 检查处拒绝。
- 原夹具与新增夹具均通过两次确定性 C 发射、JIT、module 和独立 AOT。
- GCC O0／O2 编译并执行实际生成 C，预热后重复三个完整模块生命周期，
  分配／释放次数、存活数量／字节，以及非法释放指标完全平衡。
- 新夹具覆盖显式／auto 初始化、重赋值、直接迭代、泛型类函数字段、
  函数和返回类型别名、Array／List／Dict／Set／嵌套容器、nullable、多返回值、
  属性／索引／工厂／条件／合并表达式取得的函数、单次求值及 catch／finally。
- 五组已有函数字段、泛型闭包、调用约定与属性回归通过 C 发射和 JIT。

独立原生 IR 合同在 lowering 前核对 21 个完整调用：18 个泛型结果、1 个
nullable 和 2 个 tuple；lowering 后注入 105 个坏合同，在 C／公共头／原生
输入头输出前拒绝，每平台 315 次空输出检查。严格的精确签名校验保持不变。

固定 536 份旧语料由 506 份成功增至 507 份，唯一恢复的是原返回容器夹具；
此前 506 份成功 C 逐字节一致，无回归。原 514 下限保持不变，本批仍未通过
整个 source admission 门。Windows 完整字符串及分配失败人口 364 项通过。

最初 r2 的 native harness 将不带 main 的旧夹具误当成脚本入口，导致两个
GCC 编译检查失败；其 JIT／module／AOT 和新夹具均已通过。r3 对旧夹具
使用其真实 library 发射入口，修正测试 host 后全量重跑，两端均 34／34。
保留 r2 证据，没有修改产品发射方式迁就测试。

## 仍待处理

另行缩减出 `typedef AliasItems = array<int>` 的直接函数返回后迭代问题，
修改前后均在 canonical iterator ABI 处拒绝，确认是既有独立缺口。本批
覆盖别名结果的保存和读取，不据此声称已经覆盖别名迭代。typed point 的
Sequence 准入问题也继续保留。

完整 74-worker 场景使用另一份已冻结的 native-global 候选继续运行；
该候选没有本批语义修改，不回填其结果，也不修改运行中的源码或报告。
已识别的 guard 问题包括过时的 Future Watch／ReadyQueue 合同、工具路径
解析和一次 O2 生命周期断言，需按完整收集结果继续归因、修复和统一验收。
本批 source profile 只重编译语义单元并同步相邻编译器源码包，复用其余
已验证对象，不能称为发布 DLL／VFS 的完整新构建。

证据根：Windows `E:/xlang-validation/five-groups-20261027/p7-shipping/`，
Linux `/home/ubuntu/xlang-validation/20261027-p7-shipping/`。

| 路径 | 内容 |
|---|---|
| callable-result-cohort-r1 | 语义对象、完整 source profile 和同步源码包 |
| callable-results-check-r2 | 初次 harness 入口错误及通过项 |
| callable-results-check-r3/results.json | 各平台 34 项最终结果 |
| callable-result-corpus-r3/results.json | 相同 536 份语料前后对照 |
| callable-result-strings-r4/results.json | Windows 字符串完整 364 项 |
| callable-result-proof-r6.json | 独立 reader 核对来源、对象、日志与产物 |
| callable-alias-before.log／callable-alias-after.log | 既有别名迭代缺口的前后复现 |

持久工具为 `tools/check_xlir_callable_container_results.py`。长期任务仍为
active，最终联合门未通过。本批仅普通离线语言／ABI／生命周期验收。
