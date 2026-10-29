# Dict 统一 IR 导入与原生持久化

Dict 的语言消费导入和封闭原生持久化恢复已接通统一 IR；生成 C 只
声明配方并调用驻留入口，不包含恢复或图遍历算法。本批不是三组
53 模块整体完成，也不更新 shipping 二进制。

## 类型、效果与拥有合同

typed Dict 从动态值或 erased 同族容器导入时，IR 必须具备完整键策略、
元素 schema、名义身份集合、原生工厂种子及 consuming 源操作数。
结果使用 checked 错误作用域和紧邻错误测试；验证器在输出 C 前拒绝
损坏合同。失败不替换目标，不改变源对象名义身份，也不发布部分结果。

Array／List／Set／Dict 的嵌套链使用同一声明配方；源根独立导入，同域
共享子对象仍通过既有引用映射保持共享。文本键传递完整指针及长度，
不以 `strlen` 推导，不将空键、NUL 键或 UTF-8 键转为整数。

JSON／XLON 的非泛型类目标使用封闭恢复图：Dict 节点有真实载体、
提供方、元素和工厂种子。nullable、char、bytes、继承类及混合容器
复用同一事务。wire 形状或值域不匹配可返回 null，原生失败继续 checked
传播。文本往返保证数据，不承诺引用身份；没有新增泛型 Dict 直接 parse。

## 两个实际边界修复

类默认 Dict 进入恢复读边界时，旧边界未声明其原生拥有适配器，实际
JIT 拒绝了有效输入。集合提供方现在声明真实 `OwnershipAdapter`，
共用引擎按恢复图认证所有者；识别所有者不会豁免其子对象图检查。
List 和 Set 使用相同能力合同，不另建 Dict 专用恢复算法。

类内只有 Dict，或 Set 内嵌 Dict 时，旧投影选择只考虑 List／Set，
实际 XLON 序列化无法表示原生 Dict。IR 类型图现在声明所需图提供方，
Cgen 输出同步使用的数据计划；Dict-only 只声明 Dict getter，未带入
List／Set。compact List 复用已有提供方，未复制算法。
宿主驻留目录将 `xl6PersistenceProjectNativeV1` 从私有 AOT 表迁入共享
声明表，避免重复登记；新增 `xl6PersistenceListGraphProviderV1`，
保证 JIT 与按需 AOT 使用相同入口。实际新增一个导出函数。

## 验收与证据

最终选择源码与实际编译输入、头、对象和产物逐项哈希核对；不同快照
仅在证明所用输入一致后复用证据。Windows 完整源码构建 295 个对象，
Linux 294 个对象。语言导入、既有 Dict／Set 回归和原生导入／恢复
集中验证；生成程序使用独立按需原生闭包，不链接完整编译器对象。

- 严格 C11 O0／O2、确定性 C 输出、JIT 和裁剪 AOT。
- 空／NUL／UTF-8 键、整数窄化、源不变、共享子对象、混合容器、类、
  nullable、char 与二进制数据的 JSON／XLON 往返及错误边界。
- 158 个损坏 IR 合同在输出 C 前拒绝；恢复节点载体、种子、元素、
  名义身份和错误分支均有真实篡改对照。
- 语言导入两个用例在两种优化级别执行四条完整分配故障前缀，直到
  第一条未命中注入的成功运行；另编译禁用拥有适配器的错误实现并拒绝。
- 原生 Dict 回归每端 64 条完整故障前缀、四个编译错误实现；共享恢复
  引擎另运行原有 List 恢复与操作前缀回归。
- Linux 所选生成程序在 O0 执行完整 ASan／UBSan，O2 独立严格运行，
  所选原生回归闭包也通过 ASan／UBSan；不等同于整个
  编译器或 shipping 动态库已完成 sanitizer 验收。
- 驻留目录验证 256 个组件、1354 个实际导出、65536 个依赖对及
  13 个负对照，没有新增组件或供应方算法改动。

最终 Dict 语言专项、Dict／Set 相邻回归与驻留目录：
`E:/xlang-validation/stdlib53-b201-verify-win-dev9/` 与
`/var/tmp/xlang-stdlib53-b201-verify-linux-dev9/`。原生证据：
`E:/xlang-validation/stdlib53-b201-native-win-dev2/` 与
`/var/tmp/xlang-stdlib53-b201-native-linux-dev2/`。
选择源码及产物证书为两端 `stdlib53-b201-index-*-dev20.json` 与
`stdlib53-b201-products-*-dev20.json`。

原有文档索引 14 项诊断和两个文本静态 guard 的历史首错保持不变；
对照证明本批未新增失败，不将这些 guard 宣称为全局通过。
Windows dev4 的并行编译因主机内存不足中断，保留该失败日志；最终
dev5 使用串行编译复测。负对照脚本的头搜索路径遗漏随后修复并重跑
主专项，不能将错误实现编译失败冒充运行时拒绝，也不能用部分成功
替代完整验收。
Linux 的大型生成用例曾在 O2 与 sanitizer 同时启用时触及编译超时，
保留日志；最终将优化级别运行检查和 O0 完整 sanitizer 检查分开，
不宣称两档优化均 sanitizer 通过。

## 未完成范围

直接槽优化、Dict-only／Set+Dict 类的深复制提供方完整性、完整资源
策略、性能和发布总门继续。深复制 ABI 选择本批保持旧语义，不能用
持久化投影通过替代所有混合图 clone 验收。三组 53 模块整体测试及
shipping VFS／动态库验收仍待完成；19 个未决策组不扩展，对象池最后。

Set 的已批准规则不变：基础值按内容，可变对象和容器按引用身份。
