# 普通产品测试清单与恢复凭据迁移

现有并发、密码学、X.509、TLS、网络普通用例合并运行；输入夹具可来自显式的
不可变控制目录，但编译器、运行库及私有 SDK 始终来自指定的真实普通产品。
`--controls` 不允许替换 provider 或加入源码对象。批次收集全部失败，不在
某一用例失败时终止剩余用例。新清单另包含 cryptoAdvanced 的 3 正例与 2 负例，
以及当前恢复策略的 6 个普通用例；不是新增攻击或密码学安全专项。

2026-10-27 首轮两端受到外部中断，保留原报告。Windows 完成 150 个用例、
记录 818 阶段；Linux 完成 191 个用例、记录 1,050 阶段。这些只是部分证据，
不宣称批次通过。`interrupted-public-r1.json` 独立核对已有完成记录和输入。

发现三份旧夹具仍使用无证书链的 `TlsResume.fromParts`，却要求恢复成功：
`tls_client_resume`、`tls_client_resume_stream`、`trust_rebind`。
按已批准的新合同为这些正常凭据显式补上实际服务端证书链，不改运行时策略，
不将 resumed=true 的断言改为 false。修复后，原普通产品两端各 19 阶段通过，
涵盖 JIT/module、严格 C O0/O2、独立 AOT 与 CLI；完整批次仍需重新完成。

`check_tls_resume_fixture_sync.py` 对两份生成快照检查完整语言文本和完整证书
TBS／外层算法字节，并独立验证所有正常证书签名。生成器使用固定公钥身份，
但重新签名的 ECDSA nonce 不固定，因此只排除已经验证的签名字节，不能假称
每次生成的 DER 都逐字节相等。该静态检查不能替代实际语言用例。

证据根：`F:/xlang-validation/five-groups-20261027/p7-shipping/`，两端
`resume-repair-*-r2` 为真实产品复测。SDK 闭合修复后的新普通构建、完整批次、
嵌套编译和参数化总门仍分别验收，不将本批测试修复写成五组全部完成。
