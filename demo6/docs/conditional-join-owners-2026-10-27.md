# 条件分支与空值后备的独立拥有权

本批恢复 `xlir_return_projection_owner.xl`。普通条件表达式和目标为
`var` 的条件表达式合并为一个 lowering 实现，生产代码净减少 78 行；
同时补齐 `??` 后备值的统一拥有边界。不改语义规则、XRT、Cgen、IR
布局或验证器，不增加标准库包装。

## 根因

被拒绝的语料第 101 行是 `return choose ? item : item, item`，不是
此前第 94 行的 captured-cell 返回。捕获用例已能通过，真实拒绝点是
动态值条件分支的合并。诊断过程保留实际未通过的生产者／STORE／MOVE
链，没有把不同来源的错误归为同一问题。

旧动态条件 helper 使用“转换为 xvalue”的结果直接初始化合并存储。
一个 flow-refined `var` NAME 可以合法返回其声明的物理句柄，而该 LOAD
仍是借入观察。后续从临时存储 MOVE 不能让这个初始借入凭空成为 owner。
严格产品验证因此拒绝了它；不能通过放宽 verifier 或跳过 STORE 输入
证明来解决。

普通条件 helper 已有逐分支拥有边界。现在普通／动态条件都调用
`xl6IrLowerConditionalAsType`：求值条件 → 懒求值选中分支 → 转换到
精确目标 TypeId → 按实际生产者取得独立 owner → STORE → 合并后 LOAD／
MOVE。借入值需要 retain/copy；已独立拥有的值直接转移，不添加重复 retain。

第 101 行修复后，同一语料第 113 行的 `optional ?? item` 暴露了相同
遗漏：动态后备入口转换后直接 STORE。后备分支现在也统一使用普通 typed
转换和拥有边界；保持后备懒求值、精确返回类型和原来的 present 路径。
两处修复属于同一轮发现／修复，稳定后统一复测。

## 保留原合同与真实失败路径

原产品拥有权测试保留所有断言，只增加可选的 C／头文件输出目的地，便于
源码 profile 检查器复用其经过验证的真实 IR；原无参调用仍有效。

每端严格合同结果为：12 个产品、23 个移动字段、11 个重复 owner 变异、
2 个包装后重复 owner 变异、7 个借入 STORE 变异、13 个窄化存储读取、
4 个借入 cell 读取和 769 个生产者分类。108 个错误变异全部拒绝，其中
包括 4 个路径相关 owner 和 2 个跳过获取的 CFG 变异；每端 216 次 C／头
文件空输出检查。重复生成的 C、头文件逐字节相同。

原 C host 仅替换两个 include 位置：固定公开声明头和本轮生成头。
没有改变生命周期顺序、错误码、注入位置、迭代数或内存断言。
Windows／Linux 的 GCC O0／O2 各通过 10,500 次案例、3,000 次真实诊断
分配失败和 288,600 次平衡分配，原始失败、清理失败与 finally 观察均保留。

新增独立语言程序覆盖普通／捕获变量、两边条件、present／absent、共享对象
身份、延迟后备、内嵌 NUL 与 UTF-8。192 次组合调用建立并释放 384 个
对象，后备严格调用 192 次；JIT、module、AOT、GCC O0／O2 和预热后三轮
完整模块生命周期均通过。

首次新增程序把开放 `var` 返回函数用于已推断为 `JoinOwner?` 的后备值，
不满足既有精确类型规则；已将测试函数的参数／返回明确为 JoinOwner。
没有为测试放松语言隐式转换。原有动态 `??` 借入后备覆盖仍由不改正文的
原产品 fixture 和严格 mutation 保留。

## 批量回归与证据

- Windows／Linux 各 19 项本批检查全部通过。
- 两端各 39 项枚举／Stack 回归、各 60 项属性／await 回归通过。
- Windows 完整字符串专项 364 项通过。
- 同内容哈希的固定 536 份语料：505 → 506 成功，原有 505 份成功 C
  全部逐字节一致，恢复产品返回拥有权语料，没有回退。剩余 30 个拒绝项
  仍包含真实缺口、负例与过时示例；514 下限不变，完整门尚未通过。

本轮在既有 Windows 1295／Linux 1294 单元一致源码 profile 上，仅重编
lowering 单元并同步相邻源码包。旧输入的 CRLF 原始字节由已保留的编译源
恢复后，必须与原 SHA-256 完全一致才登记别名；不改写旧 manifest 或原件。

Windows 证据根为 `E:/xlang-validation/five-groups-20261027/p7-shipping/`：

| 目录／文件 | 内容 |
|---|---|
| product-probe-r1、product-probe-r2 | 最初拒绝与实际借入 STORE 链的诊断 |
| conditional-cohort-r3、r4 | 原始换行 pin 检查、条件修复后继续发现后备拒绝 |
| conditional-cohort-r5 | 最终生产源码、对象及一致编译器源码包 |
| conditional-check-r6 | 新增测试类型不合法的首次失败，原 native 合同已通过 |
| conditional-check-r9/results.json | 最终 19 项公开／原生检查 |
| conditional-regressions-r10 | 未改变人口的枚举与属性回归 |
| conditional-corpus-r7/results.json | 固定 536 份源码对照 |
| conditional-strings-r8/results.json | 完整字符串专项控制器 |
| conditional-proof-r11.json | 独立 reader 的输入／日志／产物核对 |

Linux 对应根为 `/home/ubuntu/xlang-validation/20261027-p7-shipping/`，
保留同名源码 profile、接入检查、回归和独立证明。持久工具为
`tools/check_xlir_return_join_owners.py`，要求完整 source profile，不能用
runtime-only 或旧 DLL 替代。完整原 C-host TCC worker 和其他参数化 guard
仍归最终联合门，本批公开 TCC JIT／AOT 不替代它们的完整入口验收。

长期任务保持 active；这不是新发布 DLL／VFS 或全部 74 项参数化 guard
通过。没有网络、扫描、攻击或密码学安全性质专项测试。
