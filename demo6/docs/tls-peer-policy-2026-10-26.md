# 拥有式 TLS peer 与附加验证谓词

本批交付默认信任验证之后的语言附加谓词，不替换 XRT 验签，不提供原始
Verify hook。CRL 提供者、完整撤销覆盖与恢复凭据策略绑定仍单独待完成；
`tls_verify` 仍 pending，不将这一批当成完整 TLS 或普通产品发布验收。

## 使用及数据合同

```xlang
auto policy = TlsVerificationPolicy()
auto verifier = TlsVerifier(trust.snapshot(), policy,
    func(TlsVerifiedPeer peer) as bool
        return peer.verificationName == "example.test"
    end)
```

必须先通过原有信任库、路径、用途及参考名检查，才会执行 additionalPolicy。
`true` 只批准已经通过默认检查的 peer；`false` 表示策略拒绝。抛出语言错误
是 checked 调用失败，保留首因，包括没有诊断对象的失败，不能当作允许。

`TlsVerifiedPeer` 只能由验证成功后的驻留生产端构造，没有公开裸指针构造、
setter 或借用入口。role 是对端角色，verificationName 是验证参考名，不是
冒充 SNI。verifiedAt 原样使用固定快照的微秒 time；不升级独立 XRT 的时间 ABI。

完整 presented chain、selected path、独立信任锚及参考名存于一个拥有 packet。
certificateCount／certificateAt 与 pathCount／pathAt 区分原始呈送顺序和实际
选中路径；anchor 单独提供。名称返回独立 string，证书返回独立 Certificate。
因此可以保存 peer，或在其释放后继续使用取出的证书，不留下原生回调视图。

## 通用 IR／runtime 合同

新增明确的 `CLASS_PREDICATE_V1` 隐式上下文合同：精确一个受管原生类参数、
默认参数模式、borrow 物理实参、精确非可空 bool 结果及 checked closure ABI。
它与既有“类 → 可空类”合同分开，不从符号名、NULL result owner、诊断文字
或生成 C 的值形态猜测结果。错误形态在 IR 验证阶段拒绝，C／头文件输出为空。

驻留通用 producer 复用已有普通 V2 闭包和拥有布局，保留真实 target closure
及参数 schema 的真实 CodeLease，独立复制字段名；没有新 opaque callback box、
生成 factory 或伪 CodeOwner。创建、调用、回滚使用同一套资源路径，既有可空
类结果 ABI 不变。布尔结果直接用 bool 输出，仅在 checked 成功后发布。

Cgen 只按这套通用枚举输出上下文、策略声明及调用，没有 TLS 专属函数体：

```c
value = xl6TlsVerifierCreateCallbackV1(
    (XL6_NativeClassResultContextV1){ /* 已封闭的字段、lease、schema */ },
    trust, policy, callback);
```

Verifier 的原生 Context 真正拥有 driver；Verifier／ClientContext／Session／
传输消费者图沿用上一批实际引用链。参数对象从 driver 的真实 lease 派生，
不在 Close 后重新取得一个新 owner。peer 自身仅拥有不可变原生字节，是可审计
的 native leaf；不把含语言闭包的 Verifier 重新当作 leaf。

## 效果与执行线程

构造只保留闭包，不执行谓词；仍为 pure、nonblocking、可失败。verifyServer／
verifyClient 可能执行任意语言谓词，移除 nonblocking 承诺。Verifier 与
ClientContext 的最后释放可能销毁捕获对象，移除 no-raise／nonblocking，
与已有 Session／ServerContext 一样走 checked foreign scope。peer 只读 getter
保留 pure／nonblocking，peer 自身销毁不执行用户代码。

回调在原生验证调用线程同步执行，不隐式分配线程或强行串行化。同一 Verifier
可被不同调用者使用，捕获可变状态的谓词必须自行遵守并发规则；用于事件引擎
时应保持短小，阻塞操作交给显式阻塞设施。新增 adapter 不改变执行器调度合同。

## 验收边界

定向 runtime 夹具使用真实正常 RSA 路径、真实 closure／CodeLease：检查五种
谓词结果、两个独立 owner 关闭后的实际寿命、独立 peer／证书，以及创建和调用
的 192 个分配失败位置。它不是语言程序、整套 TLS 或 shipping 证明。

完整源码 driver 另外检查正常 RSA／P-256／P-384／Ed25519 xlang 程序、谓词
拒绝／抛错、空程序裁剪、typed／lowered IR、严格 C、JIT／module／AOT 和
checked 退役。独立 IR 反例守门、既有类结果回调回归与最终结果均应以保存报告
为准，不能仅因编译成功就称整批通过。

实际公开接入的 Windows 121、Linux 107 个阶段按预期通过：四族正常 server、
策略拒绝／抛错、真实语言 capture 环、空程序裁剪、client-auth 的空参考名与
独立证书、七种 IR 合同拒绝／空输出及既有类结果回调。另有两平台 runtime
七配置、20 阶段和每配置 192 个创建／调用分配位置，堆账本平衡。

Windows 初次 IR 反例夹具未恢复共享数组元素，失败记录保留；只复测两项 IR
夹具及新增环，没有重复已经通过的七个普通程序。旧 Verifier 清单因新增第三个
构造入口从 19／6 变为 20 targets／7 owned，仍检查原有两个证书数组、两个
策略，并明确检查新增唯一的精确 bool profile；不是删目标或放宽断言。

Linux 的原生候选目录在验收中消失；从已保存归档逐字节恢复同一候选及 TCC
支撑输入，保留最初失败，随后只执行失败／未执行的 26 阶段，补齐 97 阶段。
不是替换 provider 或改成不需要该依赖的测试。

验收期间工作区还有独立的 AST、IR、Cgen／FFI 更新。使用完整构建时已经保存的
副本及输入哈希，将原始 2,512 个生产输入封入隔离目录；两个 Linux 特有输入
另按原收据补齐。对象、可执行程序和原始编译收据不变，只重定位相同输入。
本节结论针对这一冻结 cohort，**不证明并行更新后的当前 HEAD 已联合通过**；
后续必须再做整合验收，提交也只包含本批增量。

客户端首个用例还触发既有语言限制：不能重新绑定捕获的 class 局部变量。
保留该明确拒绝，用已支持的状态对象字段完成相同 TLS 数据／寿命检查；没有
通过 var 擦除类型或添加 TLS 特例来伪称已支持通用 class shared-cell。

证据根 `F:/xlang-validation/five-groups-20261026/p7-tls-peer-policy/`。
无 socket、外部 DNS、公网、畸形密码材料或网络安全性质测试。没有改 release
二进制；后续仍须普通 DLL／SO、CLI、VFS／嵌套编译与五组联合验收。
