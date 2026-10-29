# X.509 公开面覆盖核对（2026-10-24）

2026-10-26 已重新核对当前源码的四集合 342 项闭合、96 原生 API／84 驻留
直接引用／12 组合归属，并补做双端各 252 个普通产品阶段。全部 342 个入口
有生成 C 调用位置见证，其中 35 个是 native 清理；这不是逐入口执行计数。
详见 [普通产品回执](x509-public-closure-2026-10-26.md)。以下保留原静态审计，
历史待办由长期任务接续，不以静态表独自关闭总目标。

此项是功能归属和声明闭合的静态核对，不是新的运行验收、密码学审计或发布
完成声明。基线为身份接入提交 `7b42c8d28`；运行验收范围仍见各批次专项。

## 公开面闭合

342 个驻留入口的语言声明、C ABI 声明、实际导出登记和 XLIR 合同集合完全
相同，各集合没有重复。338 个为分组直接合同，另外四个路径入口使用共享的
native collection 合同，精确描述 `array<Certificate>` 及 checked 回调；没有
遗漏到保守外部调用的备用路径，也不通过 Cgen 猜测数组成员拥有者。

21 个模块的逐项语言归属保持在 [接入总表](x509-managed.md)。数据、结构化
记录、用途／约束、签名、CRL 策略、路径、建链、信任来源和身份都有对应
语言能力；不因原生函数有多个便利写法就再建立重复的语言状态或借用接口。

固定头文件中 X.509 共 96 个 C API。其中 84 个被驻留适配直接使用。下面
说明其余十二个的归属。旧 TLS Cgen 的原始 AddFile 引用现已清退；2026-10-26
扫描当前 Cgen，对全部 96 个 X.509 原生 API 的调用形式引用均为零。

| 原生 API | 已有语言能力及不逐个包装的原因 |
|---|---|
| `xrtX509CrlConfigInit` | `X509ValidatedCrl.fromCrl` 使用显式时间、skew 和严格默认 flags 构建完整配置；不暴露可变 C 配置 |
| `xrtX509CrlStatus` | `fromCrl(...).query(target)` 分别调用原生 validate／check，结果独立拥有并可复用；原生便利函数本身也是这两步 |
| `xrtX509ExtensionInit` | Certificate 的 Extensions DER 交给通用 ExtensionListInit；支持证书／CRL／独立列表，共用拥有记录 |
| `xrtX509NameFind` | `X509Name.find/findOid` 查询已经由原生解析建立的属性索引，保留首项及 DER 顺序；不重新解析，不把正常缺失转成异常 |
| `xrtX509PathConfigInit` | `X509PathPolicy` 显式时间、用途、flags 和预算产生不可变完整配置；不提供 C 配置指针 |
| `xrtX509PathValidate` | `X509Path.check` 使用原生 PathCheck，保留 typed status／reason／depth 和成功路径，而非只给 bool |
| `xrtX509PathBuild` | `X509Path.build` 使用原生 PathBuildCheckLimit，额外保留搜索预算及结构化结论 |
| `xrtX509PathBuildCheck` | 同上；不用没有工作量上限的旧便利入口 |
| `xrtX509RevocationInit` | `X509RevocationCheck` 的独立拥有状态由 calloc 建立原生零状态，后续 update／result 仍调用 XRT |
| `xrtX509StoreAnchor` | `anchorAt` 从同一稳定版本的证书建立独立拥有锚，不把 native store 内部指针交给语言 |
| `xrtX509StoreAddSystem` | `addSystem` 锁外建立原生系统源，再事务合并；旧快照稳定，不在目标锁内做 OS 枚举 |
| `xrtX509StoreAddFile` | `addFile` 先执行有硬上限的原生文件读取，再交给共用 StoreAddData；原始便利接口没有该读取上限 |

这些归属没有替换密码／ASN.1／名称／路径算法。索引查询、配置构造、数据
拥有和限额 I/O 是语言适配责任；原生安全算法继续使用 XRT。

## 尚未关闭的边界

- 新身份代码只做了身份及受影响的数据／profile 组，不把之前来源基线的九组
  回归标成新代码的总回归；最终跨组联合验收留在 P7。
- TLS 的默认链／名称／撤销组合、不可变配置快照、Future 及票据是后续 P6
  批次的范围，其最新进度以长期任务为准；旧信任来源 Cgen 引用已清退。
- 新增原生符号的静态目录映射、旧公开路径／文档收口、发布产品及两项历史
  Windows 间歇诊断仍未关闭。没有宣称整个 P5／长期目标或 shipping 完成。

逐项源码见证、四集合比对及输入哈希保存于
`F:/xlang-validation/five-groups-20261024/p5-x509-identity/surface-audit.json`。
原生 API 引用矩阵只是源码辅助索引，不能单独证明语言功能已经通过运行测试。
