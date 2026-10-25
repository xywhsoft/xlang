# 五组接入 P7：正常业务流的真实产品联合验收

## 本批范围

`stdlib_five_groups_joint.xl` 把五组已接入能力用于同一次普通业务流：

1. 固定 TaskPool、Atomic 和原生 Channel Future，传递拥有式 bytes 结果。
2. 使用固定 P-256 私钥对 SHA-256 摘要签名，并只验证正常结果。
3. 使用已有固定 DER 证书链、TrustStore 快照和显式验证时间建立 TLS 上下文。
4. 在字面 loopback 地址、系统分配端口上建立自己的 TLS 服务端/客户端，
   收发摘要和响应；不解析公网名称，也不连接第三方服务。
5. 显式关闭任务池、连接、Listener 和 Engine；公共宿主销毁模块和上下文后，
   检查 `xlRuntimeDrainRetiringChecked` 的 ready 与 ok。

select 和 Windows IOCP / Linux epoll 分别实际运行，并检查 Engine 返回
的后端身份。这里不以静默回退替代原生后端，也不把本用例当作全部网络后端、
全部 API、全部拥有图或安全专项验收。

## 真实产品与可复现入口

复用前批正常构建的真实 CLI、运行库和编译库；其构建归档及来源见
[候选验收](five-groups-p7-candidate-2026-10-25.md)。没有 source-object
替换，没有修改算法、时间 ABI 或生成代码来适配测试。

```text
python tools/check_five_groups_joint_shipping.py --root <正常源码构建目录> --build-report <build.shipping.json> --out <新证据目录>
```

宿主仅使用公共 `xlc.h` / `xl.h` 并链接完整编译库。一次编译后在同一单元
上输出 typed IR、lowered IR、C 和 AOT，然后分别完成 JIT 与显式模块运行，
保留每条 API 自身的验证，避免重复解析相同输入。严格 C 使用候选 VFS
声明资源，不用当前磁盘私有头代替候选头。

验收将三件产品复制到没有 src/lib/res/include 的目录；独立 AOT 再复制到
另一目录运行，并检查其不导入 xl/xlc 共享库。CLI 也单独完成同一业务流。
每端包含 7 个控制器阶段、宿主内 8 个明确 API 阶段，以及 8 次完整业务流
（每个后端分别经过宿主 JIT、模块、独立 AOT、CLI 四条路径）。

证据：

- Windows：`F:/xlang-validation/five-groups-20261025/p7-joint/accept-windows-r1/results.json`。
- Linux：`/var/tmp/xlang-five-groups-20261025-p7-joint-accept-r2/results.json`。
- 双端输入、日志、产物及前批候选来源的复核：
  `F:/xlang-validation/five-groups-20261025/p7-joint/verified.json`。

Linux r1 在宿主执行中受到 WSL 环境重启中断。初始报告时间为 23:26:11，
后续观察到的新启动时间为 23:32:42；报告仍未完成。它不是成功证据，也
不据此判定编译器崩溃。原目录和日志保留，r2 使用同一候选和未变输入重跑。

## 尚未完成与性能发现

本批只封闭上述正常组合路径，不宣告五组长期任务、整个工作区、全项目
RC0 或所有 IDE 场景已验收完成。旧网络生成实现/fixture 清退、相关守卫
对齐和其余跨模块拥有图仍须逐项收尾。

只读批量审计从 33 项降到 26 项，其中 1 项是审计控制器的脚本路径问题，
其余 25 个断言待核对；其中不少仍要求已经驻留化的算法出现在 Cgen 内，
不能通过放松生产 verifier 或删除测试把它们标绿。完整正常守卫未通过。

真实候选上语义检查约 32–36 秒。调用栈及独立诊断对象的机器码对应定位到
`xl6IrBuildDynamicMethodEligible`：无关类的方法也反复执行全节点扫描。
诊断对象仅用于找回已剥离的静态符号，未链接到任何验收或发布产品。
下一批应先确认声明所属类的祖先关系，保留覆盖、重载和完整性校验，
再验证实际加速；本批不包含性能修复，也不声称已解决该问题。

未决策：CRT 0 项、XRT 11 组（核心/所有权、内存、动态值图、剩余平台信息、
HTTP、WebSocket、类型转换、对象图、调用值桥接、typed 容器、内部支撑）。
内存池仍最后处理。没有公网探测、攻击模拟或网络安全性质测试。
