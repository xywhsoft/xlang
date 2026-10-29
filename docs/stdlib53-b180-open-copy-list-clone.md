# 开放复制与 List 递归克隆

开放 `var` 的 `copy` 现在独立复制原生 List 的存储。`clone` 的
List 适配器已接入语言生成代码，并与 Array、wire 容器共用一次
XRT 图遍历和一个 memo；重复引用保持共享关系，不再沿不同边
独立复制同一个子对象。本批不改变公开语法或新增标准库 API。

## 复制合同与组件选择

XLIR 的 `xl6IrValueCopyProviderAbi` 根据已验证的物理类型决定提供者。
浅复制的已知 List 继续调用 `xl6ListValueCloneV1`，开放值调用
`xl6ValueCopyV1`，已知 Array 保留 `xl6ValueCloneV1`。这些入口都
借用输入、返回拥有结果，保留真实的布局、元素准入和投影策略。

递归克隆使用 `xl6ValueCloneGraphV1`，以栈上只读计划选择真实
List provider。该计划不注册全局回调，不建立第二个图 memo，也
不把 List 转成 wire 后再克隆。静态可证明不含 List 的叶值 Array
保留原有图入口，完整组件计划排除新复制入口、List graph 和
List native。开放、递归或不透明域采用保守的提供者闭包。

Cgen 的同步、async 和 generator 路径消费同一 XLIR 合同，只
输出驻留调用，不追溯 AST 或生成复制算法。返回失败统一通过
MAY_RAISE 和 NULL_RESULT 合同处理。旧 Array 复制入口建立错误
作用域失败时改为 checked 失败，不再直接 abort。

`copy` 仅隔离容器存储，引用槽仍按浅复制规则保留子对象；
`clone` 隔离递归容器并保留重复边的共享关系。字符串保留精确
字节长度，包括内嵌 NUL；List 键保留完整 int64 范围。BUSY 和
未绑定投影的真实 List 被拒绝，外来 handle 不按 tag 伪装成 List。

## 双端专项验收

Windows 编译器从 270 个翻译单元构建，Linux 从 269 个翻译单元
构建；复用部分仅为供应 TCC 的厂商引擎。生产输入来自隔离的
Git index 快照，不包含工作区中的其他 IDE 和扩展库修改。

每端专项通过 167 个阶段、70 条完整故障前缀、528 次逐点尝试。
每次尝试独立关闭分配账本。测试包括 O0/O2 的原生输入、开放
复制、嵌套 List、wire/native 混合图、重复引用、精确文本、
OOM、无诊断失败以及 finally 恰好执行一次。20 项损坏 IR 在
C 与头文件输出前被拒绝；两项真实实现负对照先编译，再被
同一测试拒绝，分别覆盖浅复制别名和遗漏 List graph provider。

确定性 C、实际 JIT、async/generator 严格 C 与执行通过。裁剪
测试按模块的实际 IR 计划构建并执行。原生输入另有外部调用方
工厂，其字符串投影回调来自 `xl6_string_value.c`，报告单独记录
为 `CallerProviderSources`，不混入模块的 `Sources`。这不是只靠
完整运行时链接掩盖缺失依赖的测试。

Linux ASan/UBSan 仅覆盖专项选定原生提供者，不认证整个编译器
或所有生成程序。正式专项报告为
`E:/xlang-validation/stdlib53-b180-copy-win-final2/results.json` 和
`/var/tmp/xlang-stdlib53-b180-copy-linux-final2/results.json`。
最终测试 fixture、host 与 runner 通过显式 test-root 和哈希记录；
生产快照没有替换。早期失败报告不计为通过证据。

双端存量回归报告位于
`E:/xlang-validation/stdlib53-b180-regressions-win-final1` 和
`/var/tmp/xlang-stdlib53-b180-regressions-linux-final1`，全部通过。

| 回归门 | 每端通过阶段 |
| --- | ---: |
| 拥有值索引与 continuation | 27 |
| 细化容器键 | 33 |
| List 原生快照迭代 | 64 |
| 存量 foreach | 64 |
| List 方法与 continuation | 84 |
| Closed/Open 恢复与分配失败前缀 | 96 |
| 静态 List 索引 | 141 |
| 开放索引与查询裁剪 | 166 |
| List 浅复制与代数 | 126 |
| List 图投影、混合图、循环与提供者故障专项 | 18 |

最后一项仅为选定图场景，不是完整原生 List 测试集。完整驻留
目录双端通过 231 个组件、1,221 个精确导出及 13 个负对照。
文档门仍有 14 项历史拒绝，本批新增为零，不是全仓文档门通过。

## 剩余迁移边界

List 源工厂、cast/准入和持久化/恢复仍待联动切换。封闭
`Array<List<T>>` 的构造 recipe 尚未开放，本批语言嵌套 fixture
使用 `Array<var>`，另用真实原生 List 输入验证递归图。
DynamicFields 和恢复隔离等内部消费端仍需逐项迁移，不能由
公开 `copy`/`clone` 的通过推定完成。

后续 [动态字段复制与恢复隔离的 List 接入](stdlib53-b181-dynamic-fields-list-copy.md)
已完成这两个内部消费端的共享 memo 与原生 List 所有权接入。
List 源工厂、cast/准入和持久化 wire codec 仍未完成。

Set/Dict 迁移、发布 DLL/SO、VFS、打包 AOT 和三组总验收仍需
继续。JSON/XLON 共享默认子对象只按声明 clone 能力隔离，
否则 checked 拒绝，不修改外部对象；不新增隐式 clone 推断。
CRT 未决策为零，XRT 19 个未决策组以及对象池最后不变，见
[标准库决策清单](标准库决策清单.md)。
