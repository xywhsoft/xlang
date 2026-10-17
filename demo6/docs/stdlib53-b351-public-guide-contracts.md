# 现行公共文档合同

批次 B351，2026-10-17。修正三组标准库完成审计发现的公开说明缺口，
不新增 API、不修改生产实现，也不以文档检查替代真实产品执行。

## 修正范围

《标准库设计.md》的主说明存在四组已落后于实现的合同：

- 数字解析仍保留仅 text/base 的早期签名及“尚待实现”。现统一为实际
  flags/size 默认参数、基数自动识别选项、特殊浮点值、完整消费和错误语义，
  补充基本数字写出及 Buffer 追加的关系。
- signal 只写基础处置，未写已实现的脚本交付。现补齐 on/once、拥有事件、
  SignalSubscription/SignalHandle、注册线程显式泵、channel 和取消路由，
  明确失败消费、可重试记录、合并饱和、线程亲和与进程全局处置的区别。
- Buffer 主表仍写增长步长构造、clear 释放容量、int 迭代、NUL 拒绝及不存在
  的 appendBytes。现同步所有公开方法名称、值复制构造、容量、uint8 快照、
  精确 bytes/string 出口、范围与前缀长度规则，单列可信原始指针互操作。
- os.exec/run 别名仍写默认超时 0。现同步实际 -1、size 和共享进程门面；
  start/startFile 也显示实际长度参数，chain 不再误写为独立 xrtChain 实现。

只处理已批准的三组能力；其他 CRT、文件库、网络及历史迁移记录不因本批
改写为完成。主说明与专题附录均须区分当前合同和当时的验证范围。

## 检查与证据

`tools/check_stdlib_guide_contracts.py` 从实际嵌入声明提取三十个 Buffer 方法名、
十五个 signal 命名空间入口及订阅／泵方法，检查主说明没有漏项。名称完整性
不等于全部重载语义证明；本次另外检查默认参数、NUL、错误、容量和线程合同。

原有十五项控制保留，新增十八项反向控制，覆盖退回旧解析签名／状态、吞掉
MemoryError、遗漏 signal 入口／方法、跨线程自动执行、路由丢事件、Buffer
旧构造／清空／文本／迭代语义、漏方法、原始指针安全承诺及错误的 os 超时／后端。
Windows、Linux 各三十三项控制通过，旧 HEAD 主说明在新检查下被明确拒绝。

九份既有夹具为 number_values、buffer_core/value/snapshot、signal_dispatch/
dispatch_lifetime/dispatch_routes/subscription_only 和 process_os_alias，均带
`stdlib_` 前缀。逐份核对当前源、冻结源、B350 实际产品清单中的输入摘要、两条
JIT／模块执行记录与日志，并复核实际产品摘要和两份被引用的标准库声明。
这是既有产品证据的关联复核，不是本批重新执行九份夹具或重新构建编译器。
未修改旧 B350 父报告，使其冒称执行了本批新版文档检查。

当前检查：

```text
python tools/check_stdlib_guide_contracts.py --root <B350-frozen-demo6> --out <new-directory>
```

Windows 文档报告为 `D:/xlang-validation/stdlib53-b351-guide-win-r2/results.json`；
Linux 为 `/var/tmp/xlang-stdlib53-b351-guide-linux/results.json`。
独立摘要复核分别为 `D:/xlang-validation/stdlib53-b351-certificate-win.json` 和
`/var/tmp/xlang-stdlib53-b351-certificate-linux.json`。初版 Windows 文档检查也
通过，但最终报告使用 r2，其中额外覆盖现行数字 flags/size 合同。

本批只确认公开文档与已实现合同的这些缺口已修复。53 模块最终逐项完成审计
继续；Windows DST 按用户决定独立待验收，不修改宿主时区或标为通过。
CRT 未决策为零，XRT 其余十九组及对象池最后的顺序不变。
