# 持久化 boxed 投影、原生配置与批量验收

本批压实已批准的 JSON／XLON／JSONL／XLONL 接入，没有新增公开
函数、修改供应方 XRT 或扩展未决策范围。主要修复两个运行缺陷：
标量投影误用未装箱的复制 ABI，以及配置读取没有识别原生 Dict。
这些专项不能代替 53 模块或 shipping DLL／SDK 总验收。

## 实现合同

`PROJECT` 的物理输入已经是拥有的 boxed Value，逻辑 payload 可以
仍为 string／整数。原 Cgen 按逻辑类型查 native-copy provider，在
有效标量程序中生成 `(null)(...)`。现在由验证过的 XLIR 指令给出
wire／native-graph provider；Cgen 只消费该合同，非法合同拒绝输出。
没有重新推断源语义，也没有增加第二层 IR 或按库名补特殊算法。

配置字典从旧 Object 转为原生 Dict 后，选项读取仍只接受旧 Object，
导致配置、嵌套 record、回调和文件入口失败。现在共用已认证的
Dict 操作 pin、精确字节键遍历和拥有值读取；键遍历没有快照或
逐键分配，嵌套选项复用同一条路径。每个读取 owner 都 checked
消费，主失败，包括没有诊断的失败，不被后续 Drop 覆盖。

公开 XRT 模块根与私有 resident 符号分别收集，不把链接符号写成
`XRT_MODULE_*` 宏，也不以全量链接掩盖缺失依赖。专用 Writer
操作、JSON pretty 布尔写文件和 XLON 默认写文件，不因本次修复额外选入配置 Dict；
实际使用选项的路径才选入。独立标量写文件用例验证其裁剪计划。

## 测试与旧断言迁移

原 ABI 门检查旧字符串侧车、旧 per-class Cgen 函数名，并错误地
要求序列化 no-raise。新门使用真实当前源码驱动，核对 13 个公开
调用、字段分区／改名、may-raise、四个标量 Box→Project 事务和
拥有值清理；保留严密的 schema、IR 损坏拒绝及确定性输出检查。

Set 恢复夹具按已批准的可变对象引用身份规则验收：不同恢复对象
不会按内容合并，独立解析的新对象也不作为原对象的同一键。
普通缺失文件仍返回 nullable；内含 NUL 的非法 C 路径 checked
拒绝，测试只消费预期 ValueError，不吞掉注入的 MemoryError。

`check_stdlib_persistence_batch.py` 一次收集全部独立结果：16 个
语言用例，20 个投影／恢复计划用例，以及闭合／开放恢复图专项。
语言门执行严格 C O0／O2、真实 TCC JIT、独立 AOT 及导入表检查。
同步分配故障从首个逻辑点逐项扫到第一个无故障点，在同一进程
内重复初始化／清理；图合同用例独立并行，不减少变异、故障点
或 O0／O2 覆盖，不放宽错误。

Windows 304、Linux 303 编译单元的完整源码驱动与 SDK 输入固定，
没有对象替换或 released 编译器回退。原生 Dict 双端各 70 阶段，
覆盖精确 NUL 键、零分配遍历、重入拒绝、静默失败、拥有读清理，
46 个前缀 lane 和 8 个可编译坏实现控制。Linux ASan／UBSan／LSan
插桩真实选中 Dict／XRT 闭包，不声称整个编译器已插桩。

证据：`E:/xlang-validation/stdlib53-b226-check-win-dev13`、
`/var/tmp/xlang-stdlib53-b226-check-linux-dev13`；原生专项见同一
验证根下的 `stdlib53-b226-dict-native-*-dev7`。提交前把实际输入、
日志、产物与最终选择性源码再逐项核对；旧失败目录保留。

## 尚未完成

静态总门诊断从 44 条降为 33 条，这是诊断收集，不是总门通过，
33 条也不等于 33 个运行缺陷。正常总门仍在 collection merge
断言处停止。余项包括容器、字符串／索引、同步、退役和测试接线。
成对迭代、原生拥有值消费、WaitQueue 拓扑及 backend 切换、更广
故障／并发压力、shipping DLL／SDK 与三组总验收继续。
CRT 0、XRT 19 个未决策组和对象池最后的顺序不变。
