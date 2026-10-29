# 类身份与开放动态字段验收

本批修复 boxed 对象字符串索引漏读动态字段的真实缺陷，统一点访问
与索引访问的常驻存储路由，并按已批准的规则更新 Set 回归：基础值
按内容，可变对象／容器按引用身份。类自身的结构化 `==`／`!=` 仍
保留，并不因为 Set 键策略改变而改为引用比较。没有新增标准库公开
函数，没有修改供应方 XRT；53 模块与发布总验收仍未完成。

## 动态字段从 IR 布局到常驻访问

原 erased 索引只读普通 Object 槽，点访问却从模块局部类布局找
DynamicFields。两者不一致；后者还以可变 runtime-tag 字段判断
对象，并直接把未核对 Ops 的 handle 转为原生指针。没有本地开放
类布局的消费模块也会错误地退回普通槽存储。

现在生成的 Class V3 schema 携带已验证 ClassLayout 的存储／handle
字段路径。开放根类和开放派生类分别取自己的 IR 路径，闭合布局
为零路径。生命周期的真实 CodeLease 保持 schema 与名称字节存活，
包括 COW／deep backing 和消费模块不持有原模块声明的场景。

`xl6_value_object_fields` 是独立裁剪的常驻 provider。声明槽优先；
缺失时认证 Class V3 生命周期，读取精确路径，再核对 resident
DynamicFields 的 Ops、非空 payload 和空 UserData。未知、损坏、
BUSY／claimed／cleared 状态不会被解释成普通缺失；不会调用未知
句柄的策略回调。普通 XRT 存储检查的是物理 backing，不把 shell
证书误认为完整类证书；无原生 identity hook 的语言自定义协议类
也能识别。

这是受同步约束的内部借用查找；公开 erased 索引在返回前 retain
拥有值。它不向 xlang 新增借用语法，也不把任意 C 指针认证为安全
payload。直接 raw point 的 FFI 有效性仍是调用方责任。

赋值保留已有 DynamicFields `SetRef` 语义，安装真实拥有边并在两种
结果下消费输入，不误用会隔离复制的 XRT `SetTake`。替换期间保留
接收者和旧子值，最后的子值 Drop 在 mutation fence 之外执行；
第一次操作／清理失败被保留，包括无诊断失败。失败可发生在提交
后的清理阶段，因此 checked 失败不承诺撤销已经提交的字段值。

生成 C 只保留机械调用，不再生成字段存储猜测或遍历算法。普通
不涉及开放 erased 字段的程序不带入该 provider；真正的组件计划
检查有／无两项，独立 EXE 再检查没有 xlang 运行库动态依赖。

## ABI 与旧测试的收口

V3 schema 结构增加路径描述，`size` 必须匹配新结构；V3 vtable
没有增加或重排函数。xlang 尚未发布，本批不提供旧二进制兼容：
旧 schema／运行库与新生成代码不能混用，发布 DLL／SDK 需要按
最终同一源码重建。本批全源码驱动与 standalone EXE 证据不代表
旧 release DLL 已被更新或发布验收完成。

Class gate 改为实际编译源码驱动，而不是 released CLI 或旧 Cgen
算法名称。保留 typed hash/equality modes、`@compare(skip)` 的
精确字段分区、bind/equality 计数、IR 损坏拒绝与 metadata OOM、
确定性 C、O0/O2、真正 TCC JIT、新模块和独立 AOT。

Set 的旧 fixture 期待两个不同可变对象因内容相同而合并，这与
已批准规则冲突。修正期待后同时增加“插入后修改仍能找到”的断言。
深结构比较的 checked 失败继续测试；原生 Class hash 的错误契约
由实际运行时专项保留，不用 Set 触发一个它不再执行的结构哈希。

## 验收入口与边界

`check_stdlib_identity_fields.ps1` 是 Windows 批量入口，也已接入
语言前端 suite。Linux 可执行其中四个 Python 门。公共源码、SDK、
对象与驱动逐项哈希固定，复用构建前比较完整生产输入与选择性源码。
本批新建 Windows 304、Linux 303 编译单元的全源码驱动。

动态字段门保留三个“坏实现先成功编译、再被运行拒绝”的控制项，
四个物理原生合同、七个语言用例、原生 O0/O2 全分配前缀、六个独立
EXE／导入表和有／无 provider 的真实裁剪计划。新合同覆盖非默认
且含 NUL 的路径、构造期访问、精确 NUL 键、拥有读结果、错误
句柄拒绝、重入 Drop、静默失败及主错误不被次要清理覆盖。

Class native 专项实际卸载 32 次 schema DLL／SO，在 owner 关闭后
继续从 COW/deep 对象读取路径字节，最后仍由真实 lifetime tail
触发卸载。Linux 对该闭包以及独立 object-fields 闭包分别执行
ASan／UBSan／LSan，包含真实选中的 XRT/xruntime 实现；不是只给
宿主加 sanitizer 后链接未插桩的运行库。独立 object-fields lane
不包含 generic index dispatcher，该边界由公共语言／原生门覆盖。

Windows 共通过 192 个阶段，Linux 198 个阶段；额外六个 Linux
阶段来自两个 sanitizer 闭包。接收接口的 `(status, T?)` 决定保持
不变，但这些字段／类验收不能用来标记 WaitQueue 已完成。

证据保存在 `E:/xlang-validation/stdlib53-b225-check-win-dev7` 和
`/var/tmp/xlang-stdlib53-b225-check-linux-dev7`；最终选择性快照与
这些实际输入、日志和产物在提交前再次逐项核对。先前失败复测
目录保留，不覆盖旧证据，也不把旧失败包装成通过。

静态总门的诊断从 56 条降为 44 条，保存在
`E:/xlang-validation/stdlib53-b225-dev/static-audit-dev7.json`。
这是诊断收集，不是正常总门通过；44 条不等于 44 个运行缺陷。
剩余包括持久化、容器、字符串／索引、同步、模块退役和测试接线。
WaitQueue 拓扑、拥有接收切换与更广故障／退役压力、shipping
DLL／SDK 和 53 模块总验收继续。CRT 0、XRT 19 个未决策组及
对象池最后的顺序不变。
