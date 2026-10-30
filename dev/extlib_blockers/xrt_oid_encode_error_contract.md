> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# 冻结 XRT 的 OID 失败诊断缺口

## 状态

2026-10-29，普通扩展库风险 `RDER`，当前实测涉及 `pkcs` 一包。
不是 OID 被接受，不是 XLang 静默返回空串，也没有发现内存损坏。
问题是 C/XRT 返回失败却没有设置新错误，违反本包公开的结构化错误合同。
本任务不修改冻结 XRT，不在包中复制 OID 解析器或兜底覆盖错误。

冻结 XRT SHA256：
`049C180A43597D5476F5357CC9A7D1603B439765A3E3550437F7D49B5B3E55F4`。
编译器基线、Win/Linux 三件套与 XRT 均未更新。

## 最小复现

```c
#define XRT_MODULE_ASN1_DER
#include "xrt.h"

xbuffer output = {0};
xrtClearError();
bool ok = xrtDerAppendOid(&output, (xstrview){"3.1", 3});
// 当前：ok == false，但 xrtGetError() == NULL。
xrtBufferUnit(&output);
```

若调用前留下 `XERR_IO`，同样失败后仍读到旧 I/O 错误；并非本次失败
发生了 I/O。直接 `xrtDerOidEncode` 也如此。包内 `xpkcs_encode_oid`
仅复用 XRT，表现为 `NULL` 加空错误/旧错误，未自行解析分量。

八类实测输入：`1`、`1/2`、`3.1`、`0.40`、`1.40`、
`2.18446744073709551536`、`1.2x`、`1.2/3`。这些输入均被拒绝，
但失败诊断不完整。合法相邻边界包括 `0.39`、`1.39`、
`2.18446744073709551535` 和 `1.2.18446744073709551615`，不可通过
截短或收窄无符号分量来修复。

对应完整回归为 `release/extlibs/pkcs/test/oid_errors_c.c`：22 输入，
清空/预置旧错误两状态，直接内容编码/直接 TLV 编码/包 C API 三路径，
共 132 检查加一项内存诊断，当前 133 项有 48 失败（8 × 2 × 3）。
失败缓冲原前缀及长度保持，活动内存/非法释放等诊断平衡；不是回滚缺陷。

语言回归 `test/oid_errors.xl` 的 22 例在 JIT/AOT 均通过。生成 C 表明：
`@owned string` 的原生空指针且无错误时，主线设置通用 `XERR_VALUE`。
所以语言端通常能捕获 `ValueError`，但这不能证明 C 侧有完整诊断；
也不能把这个兜底解释为 XRT 已正确提供原始原因。

## 已执行验证

- Windows GCC 16.2 `-O2 -DNDEBUG -Wall -Wextra -Werror` 和 C 直接
  `xl run` 均为 133/48；聚焦原始结果在
  `build/test/pkcs_oid_probe_e625ea9787f74d838bf684bff3ee9323`。
- Windows 默认门禁 35 阶段，前 34 成功：公开头、严格 C、旧边界/OOM、
  16 个实际生成声明与头、11 文本直接接管及完整返回无副本、全部旧/新
  XL JIT、七条独立 EXE 和双语输出。最后 C OID 诊断如实退出 1。
  40 项指纹变化 0；报告 `build/test/pkcs_guard_5c71adc2df9d4f868448bc7ca1359a6d.json`。
- Linux 原生快照 `/var/tmp/xlang-pkcs-oid-src-WjtPqW`，35 份源件复制
  前后与原位置校验一致；门禁另包含 GCC 共 36 项指纹。ASan/UBSan、
  OpenSSL 独立解析、旧新 XL、七条 AOT、双语输出均先通过，末尾同样
  C 133/48、退出 1，未见 sanitizer 报告。现场
  `tmp/xlang-pkcs-ytN9yG` 和旁边的报告保留。
- 缺少 C 编译器的诊断自测退出 127，并保留现场 `tmp/xlang-pkcs-07khNh`。
  门禁进程有界，失败时也核对指纹和保留报告；未吞掉红色回归。

## 修复责任与恢复步骤

问题位于冻结单头 `lib/xrt/xrt.h` 的 `xrtDerOidEncode`：同一个短路
条件混合内存/词法助手失败和裸语义条件，后者直接 Unit/return false，
没有赋新错误；后续循环的非点分隔符分支也相同。应在 XRT 来源仓库的
对应 ASN.1 实现修复，再按项目既定流程生成单头、审核同步，不能直接
给发布单头打孤立补丁。

建议拆开这些失败原因：保留分配/助手已有原因；缺分量、错分隔符、
首两分量规则失败显式给出参数值诊断；合并首分量超出 uint64 范围按
既有范围错误模型处理。不要在函数末尾无条件设置通用错误，那会覆盖
真实内存错误或具体溢出原因。失败仍保持原输出前缀，API/ABI 不必变。

来源侧先补空/旧错误、三调用层、合法上界和逐点 OOM；随后受控同步
XRT，再运行上述 133 项与两个默认包门禁。C 和语言的公开调用均恢复后
才能将 `pkcs` 从 R 重新签收；不能只凭 XL 的通用异常兜底关闭问题。
