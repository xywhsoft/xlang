# TLS 恢复时的证书链与当前验证策略

本批落实“共享只读证书链，恢复时重新验证”的决定，不关闭恢复功能，
也不把旧的验证成功结果或不透明 `peerIdentity` 当成当前认证决策。

## 公开合同

```xlang
ticket.peerCertificateCount
ticket.peerCertificateAt(index) as Certificate
ticket.withPeerCertificates(certificates, maxCertificates = 64,
                            maxCertificateBytes = 16777216) as TlsResume
```

默认语言验证器启用不可变的 XRT `XTLS_VERIFY_RESUME_PEER_V1` 选项。
真实服务端选择恢复 PSK 后，客户端状态机对保存的实际证书链重新执行当前
验证器的路径、信任、用途、名称、时钟、CRL 和附加谓词；旧验证器不随票据保存。
任一当前策略失败都 checked 拒绝，仍需正常 Finished 才能进入 Ready。
Session／Stream／Dial 共用 XRT 状态机，不各自维护一套判断。

`fromParts` 只有凭据数据，没有证书链。可选恢复不提供该 PSK，转完整握手；
required 恢复在创建客户端时 checked 拒绝。`withPeerCertificates` 返回新凭据，
复制并解析完整 DER，不修改原凭据、不把导入当作信任证明；恢复时仍重新验证。
预算超限不截断，负数和 native size 溢出拒绝，0 表示显式关闭该项预算。
`peerCertificateAt` 返回独立拥有的语言 Certificate，不暴露可变原生视图。

原有手工 PSK 接入夹具显式补入正常证书链，保留真实恢复断言，不通过把
预期改成“从此不恢复”来掩盖迁移。基础 `fromParts` 仍可独立使用。

## 实现与成本

完整握手原有的精确 DER 分配成为只读拥有者。客户端票据、恢复会话和后继
票据只增加该拥有者的引用，不重新复制 DER。该拥有者没有语言引用或旧策略，
所以 TlsResume 仍是原生叶资源；不是把含语言闭包的对象伪装成叶节点。

XRT 新增工厂选项而不改变旧 config／owned V1 布局，旧 C 工厂保持 flags=0。
只编入基础 TLS_RESUME 时不增加证书拥有者字段；启用 CLIENT_RESUME 后才有
共享链指针／引用计数。复验有真实路径验证、策略调用与计数成本，不声称零开销。
原生实现提交为 `1d4109aff9d52d0452742669e81caa8bef626922`。

语言适配驻留 runtime，复用通用 CLASS_ARRAY_V1 与实际 Certificate handle
准入、独立捕获和 checked 错误域。DER 索引小于等于 8 时用栈内字节视图，
更大时只分配视图数组，不复制解析结构。显式导入仍需一份新链和新凭据；
getter 按拥有值合同复制证书，不能称为零桥接成本。

公开 peer 操作目前属于 XRT CLIENT_RESUME 能力闭包；基础 fromParts 的裁剪
不能被解释为 peer 查询也只需要基础凭据代码。后续最小闭包／体积评估需如实
保留这一区别。Cgen 没有增加 TLS 实现体，只输出声明、通用 context、调用与清理。
两边原有 ms／For 与微秒／Until 时间合同不升级。

## 验收证据与剩余边界

证据根：`F:/xlang-validation/five-groups-20261026/p7-tls-resume-peer/`。

- `native-win-r2`／`native-linux-r2`：最终 DER-view 接口，模块化和单头共
  14 配置／28 阶段；包含真实完整握手、签发、required 恢复、后继共享、独立
  生命周期、当前回调／时钟、缺链处理及 128 个原生导入分配故障位置。
- `revocation-runtime-windows-r1`／`revocation-runtime-linux-r1`：新工厂下
  既有 CRL／双策略真实拥有图七配置回归，共 20 阶段、每配置 384 个故障位置。
- `peer-runtime-windows-r1`／`peer-runtime-linux-r1`：四签名族、短／堆视图
  数组、独立输出，七配置共 20 阶段、每配置 256 个捕获／导入故障位置。
- Windows／Linux 的完整源码驱动器分别编译 1,287／1,286 TU；冻结 9,240
  个源码／manifest 输入，不替换原生 provider，不把它当作 shipping 构建。
- `public-*`、`migration-*` 和 `contract-*` 是公开语言、旧夹具迁移及 IR
  空输出拒绝门：两端各 60 个公开阶段、10 个旧夹具迁移阶段、6 个合同阶段
  通过。四族都实际签发／恢复并重新执行 CRL／附加谓词，覆盖 typed／lowered
  IR、严格 C O0/O2、JIT、module 和无运行库依赖的独立 AOT。13 种合同变异在
  两端两个优化配置中均拒绝，104 次 C／头输出为空；parts／空程序裁剪通过。

合计 220 个成功阶段，不把源码构建的数千 TU 再加进阶段数。独立核对回执、
输入、日志和产物，收据为 `proof-windows-r1/proof.json`（106 阶段）和
`proof-linux-r1/proof.json`（114 阶段）；最终状态以这些收据为准。Linux 的共享
盘 9P 等待自行恢复，原进程完成原轮，没有重启验证、迁移证据或改系统设置。

只做离线正常材料、内存握手及常规 ABI／IR／拥有权／分配故障检查，无 socket、
公网、DNS、畸形密码输入或网络安全性质专项。

普通产品 DLL／SO、CLI／VFS／嵌套编译／IDE 和五组联合验收仍需后续门槛，
不能把本批 source-driver 通过写成整体 TLS 已完成；`tls_verify` 暂不封口。
未决策仍为 CRT 0／XRT 11，内存池最后。
