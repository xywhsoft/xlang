# 语义补全的编译器基础

2026-10-07 已实现真实光标补全查询 `xlcComplete` / `check --json --completion-at`，支持未完成标识符和成员点号后的查询。查询直接捕获实际词法作用域与 class 接收对象类型，提供真实遮蔽、确定赋值、重载签名、权限、继承、静态/实例、泛型和 nullable/流分析事实；源文件原字节保持不变。

它与成功检查索引分开：响应为 `cursor-query`，显式标记 `checkedCompilation: false`。查询成功不等于整份编译成功。它携带本次真实来源及解析报告，失败/超限时整批拒绝。接口与已支持/未支持范围详见 [编译器合同](../../docs/IDE语义补全查询接口.md)。

后续增量已把该查询接入标准 LSP completion、独立不可变快照与持续输入守护，支持 UTF-16 whole-token textEdit、取消和最新请求排队，服务已声明 `completionProvider`。原生候选已接入 Ctrl+Space、点号、整段提交与一次撤销；语义候选与静态目录组合，按文档/依赖/光标/焦点及持续守护失效撤销。C/xlang 双语法、大纲、悬停和 F12 保持，C 使用目录候选。详见 [原生补全](native-completion.md)。详见 [光标补全服务](semantic-completion-service.md)。C 语义服务及完整项目输入继续实施。

本次 42 项补全回归和 114 项旧接口兼容性回归全部通过。补全回归覆盖 Unicode/BOM/CRLF 坐标、半输入、作用域顺序、同名遮蔽、重载、继承、成员权限、nullable/流分析、未保存入口及依赖覆盖、公开 C ABI、选项形状路径、整批容量拒绝，以及查询失败后恢复与原上下文保持。

- [可复跑测试](../../tools/check_completion_query.py)
- [新开发工具链](../build/toolchains/demo6-completion-a96b23d8-16c9-4eae-9da7-6d082d45f5a7/xl.exe)
- [工具链哈希清单](../build/completion-toolchain-snapshot.json)
- [本轮验证汇总](../build/completion-final-verification.json)
- [共享源码合并记录](../build/completion-root-merge.json)
- [旧接口兼容性](../build/completion-legacy-gates.json)

新工具链独立冻结；共享 `release` 程序和原 IDE Release 未被替换。并行资源/IR 工作保持，编译单元只合并互不重叠的查询加载改动。当前构建列表重复输入由私有验证命令去重，这项原有输入没有被本增量改写。隔离目录首轮缺少历史测试夹具导致兼容性测试中止，补齐并逐字节验证 3902 个夹具后，四组回归完整重跑通过；首轮输出留存。
