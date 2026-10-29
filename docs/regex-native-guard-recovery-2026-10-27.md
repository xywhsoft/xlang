# Regex／Pattern 原生验收依赖恢复

工作器按登记表计算四组原生适配依赖闭包，补齐 NativeHandle 和 foreign-error
实现及 SDK 声明路径。独立 XRT provider 显式选择 Once；Pattern provider 还
选择 NativeHandle 所需的 Value 模块。所有 GCC 调用使用记录真实 argv 和
哈希的响应文件；编译器和原生对象使用构建清单中的真实路径，支持已验证的
增量源配置。正式 CLI 目录可指定，默认仍为 release。

五个原有原生程序实际编译、执行通过：Regex 68 次分配失败／5 个首个未触发
点，扩展 Regex 223／9，Pattern 60／5，Builder 49／10，回调 133／3。原来的
NUL、快照、线程约束、4000 次 Pattern 跨线程读取、异常和分配账本检查保留。

完整工作器仍失败：`Pattern.code`、`PatternMatch.capture` 和
`PatternBuilder.code` 是非空原生资源字段，在构造体中赋值，但当前语义规则
要求字段声明包含初始化表达式。错误投影合同因此在进入 IR 前被拒绝。后续
需要通用的构造期字段初始化与失败回滚合同；本批没有改动该语言规则。

原生通过范围的独立核对：
`E:/xlang-validation/five-groups-20261027/p7-shipping/regex-native-proof-r81/results.json`。
原始完整失败报告保留在 `regex-guard-r80/evidence/results.json`；本批不宣称
语言、正式产品、完整 Regex 工作器或长期任务验收完成。
