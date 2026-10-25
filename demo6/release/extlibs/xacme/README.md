# xacme

以 XRT XACME 为准的类型化 OOP 绑定。旧 `acme` 包、示例和设计稿已删除，不保留兼容接口。
协议、JWS、TLS、五家 DNS provider、Store 和 Obtain 均直接使用上游实现。
快照提交及校验值见 [source.json](upstream/source.json)，43 个公开函数的映射及
38 个业务常量见 [api-map.json](api-map.json)。快照只包含 XACME，没有第二套 XRT 核心。

```xl
import xacme

auto config = xacme.ClientConfig()
config.Account.DirectoryUrl = xacme.XACME_DIRECTORY_LE_STAGING
config.Account.ContactEmail = "admin@example.com"
config.TimeoutMs = 30000
config.IssueTimeoutMs = 120000
auto credentials = xacme.CfConfig()
credentials.ApiToken = env.get("CLOUDFLARE_API_TOKEN") ?? ""
auto dns = xacme.DnsProvider.Cf(credentials)
auto client = xacme.Client.Create(config)
try
    auto chain, key = client.Issue(["example.com", "*.example.com"], dns)
    println(chain)
    xacme.Store.SaveGrant("./cert-store", "example.com", chain, key,
        config.Account.DirectoryUrl)
finally
    assert client.Cleanup()
    assert dns.Unit()
    assert xacme.CleanupPending(5000) == 0
end
```

此示例申请证书并修改 DNS，域名和凭据须由调用者配置。网络操作与 C API 一样同步阻塞。

* `Client.Create/AccountPem/Issue/IssueEx/IssueStored/Revoke/Rollover/Deactivate/Cleanup`
  对应同名 `xrtAcmeClient*`。配置字段去掉 C 字段的 `s/p/u/i` 前缀；
  `Account.Eab.Kid/Hmac` 对应 EAB。三个配置构造器的默认值与上游 Init 的全零语义一致。
* `DnsProvider.Ali/Cf/Tencent/Aws/Huawei(config)` 创建内建 provider，每家均有对应配置类。
  `Add/Remove/Propagate/Validate/Id/Caps/Unit` 提供实例操作，`Count/ProviderId` 提供枚举。
  Unit 释放本地上下文，不删除云端 TXT 记录。
* `DnsProvider.Custom(id, add, remove[, propagate])` 接受 `func(string,string) as bool`。
  包内完成 xcall 绑定、异常传递和闭包追踪；提供传播回调时自动设置能力位。
  回调同步执行，false 表示失败，不自动重放 DNS 写操作。
* `Store.SaveAccount/LoadAccount/SaveCert/LoadCert/LoadCertCa/SaveGrant/LoadGrant/ListDomains/NeedRenew`
  直接调用上游存储接口。root 显式传入；原子版本发布、通配符编码和 ListDomains 的
  capacity 语义与 C 一致。
* `Obtain(config, domains, dns)` 返回链、私钥、是否续签。ObtainConfig 另有 StoreRoot 和 RenewalDays。
* 创建 Client、内建 provider 或调用 Obtain 时，可以增加一个 `NetEngine` 参数借用宿主引擎。
  适配器保留真实运行时拥有者，并在每次调用期间建立引擎使用域。引擎显式 close 后，
  新操作拒绝访问，清理仍可继续；不接受无生命周期保证的裸 point。

`AccountPem/LoadAccount/LoadCert/LoadCertCa` 返回 cstring；Issue、IssueEx、LoadGrant 返回
`(cstring,cstring)`；IssueStored 和 Obtain 返回 `(cstring,cstring,bool)`。产物通过 xrtFree
接管，原指针不会为返回、typed 局部别名、只读比较或输出而复制。双结果使用临时原生拥有者，
任一接管步骤失败时仍会释放其余产物。写入、var、容器和类字段按语言既定规则物化。
传入 C 的普通字符串与 cstring 均检查内嵌 NUL 后借用原字节；配置和域名数组使用显式数据快照。
Provider ID 是普通短 string，具有独立拥有的存储。

普通操作失败会抛出携带原始 xerror 的语言异常，保留 kind/domain/code/cause 和无诊断的失败状态。
Cleanup/Unit 返回 bool：false 保留原生拥有权，实例此后只能继续清理。
自动析构遇到未退休资源时，将拥有者及依赖保留在常驻队列中。
`CleanupPending(timeoutMs)` 返回剩余数量，0 才表示完成；0 毫秒只做一次非阻塞轮询，
-1 表示持续等待。它同时处理上游失败构造/Obtain 的队列与已交付实例析构后的队列。
没有后台清理线程。同一实例的重入或并发操作会被拒绝。

原生适配器通过可选常驻运行时组件接入，具有明确的句柄策略、资源图、准备和完成阶段。
resolver、私有 engine、借用引擎拥有者和语言回调均进入真实依赖图，不会被当成叶子资源。
编译器按实际依赖选入组件；源码更新后须重建 xlang 编译器/运行库。
内置 XRT 的微秒接口与上游 XACME 毫秒接口之间，由包内局部适配层转换。

验证使用本地模拟 CA/DNS，无需云凭据：

```powershell
pwsh tools/build_xlir_public_source_driver.ps1 -Directory build/xacme-driver
python tools/check_xacme.py --directory build/xacme-check --xrt D:/GIT/xrt --driver build/xacme-driver/public_source.exe
```

Linux 使用源码构建的 CLI：

```sh
python3 tools/check_xacme.py --directory build/xacme-check --xrt /path/to/xrt --cli release/xl
```

门禁覆盖原生 OOM 回滚、借用引擎、未完成清理重试、指针转移、自定义回调、模拟签发与存储，
以及基础调用、PEM 使用、保留全局资源和回调时的模块退役的 JIT/AOT 路径。
刷新上游使用 `python tools/sync_xacme_upstream.py --xrt /path/to/xrt`，随后重新跑全部门禁。
