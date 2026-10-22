# pkcs

这个包提供常用 ASN.1 DER 构件的十六进制接口，底层复用 XRT 的 ASN.1/DER 实现。名称沿用 `pkcs`，但**不是** PKCS#7、#8、#11 或 #12 的完整实现，也不提供证书、密钥解析或签名验证。

## 功能范围

| XLang API | 作用 |
| --- | --- |
| `encodeInt(value)` / `decodeInt(derHex)` | int64 与 DER INTEGER 双向转换 |
| `encodeOid(dotted)` / `decodeOid(derHex)` | 点分 OID 与 DER OBJECT IDENTIFIER 双向转换 |
| `encodeNull()` | 编码 DER NULL |
| `encodeOctet(contentHex)` / `encodeBitString(contentHex)` | 编码 OCTET STRING / 未使用位数为零的 BIT STRING |
| `wrapSequence(contentHex)` / `wrapSet(contentHex)` | 将完整 DER 子项拼接内容封装为 SEQUENCE / SET；子项必须是合法 DER，SET 子项须按完整编码字节排序 |
| `getTag(derHex)` / `tagName(derHex)` | 读取首个标签字节 / 常见通用标签名；`getTag` 不解码高标签号 |
| `getLength(derHex)` / `getContent(derHex)` / `derLen(derHex)` | 获取内容字节数、内容 HEX、整体 DER 字节数 |
| `nextOffset(hex, byteOffset)` | 从某个 DER 子项的字节起点找到下一子项的字节起点 |
| `concat(leftHex, rightHex)` | 拼接两段合法 HEX 文本；不要求每段本身是 DER |

HEX 输入不区分大小写；输出为小写。除 `concat` 和 `nextOffset` 的多项输入外，读取接口要求输入恰好包含一个规范 DER 值，拒绝空输入、截断、尾随字节与非规范编码。所有 XLang 文本参数拒绝内嵌 NUL，避免 C 零结尾接口静默只处理有效前缀。`getContent` 返回内容的 HEX，而不是解码后的字符串。XLang 失败时抛出异常，不把合法整数零和失败混淆。
`nextOffset` 校验整个多项 DER 输入；即使目标子项有效，后续子项截断或不规范也会拒绝，不返回部分可信的偏移。

## C 接口与所有权

公开声明位于 `include/pkcs_native.h`。返回的 `void*` 实际指向 NUL 结尾的文本，C 可转成 `char*` 使用；它是独立分配的调用方所有内存，须使用 `xrtFree` 释放。此处保留 `void*` 是为了与 XLang 生成的 `point` ABI 原型一致。失败返回 `NULL`，可通过 `xrtGetError()` 取错误。`xpkcs_decode_int` 使用返回值表示成功，并仅在成功时写入 `int64_t*`，因此零也是有效结果。`xpkcs_decode_int_value` 仅供 XLang 的 `var` 桥接，C 调用方一般应使用前者。
若 C 侧使用 `xpkcs_decode_int_value`，返回的 `xvalue*` 由调用方以 `xrtValueRelease` 释放。公开头可独立包含；应用需要操作该值时须自行启用 XRT value 模块。

XLang 封装通过 `__xl_runtime.takeCstr` 接管 C 字符串，无需用户手工释放。包所需 XRT 模块在 `xlang.package.json` 的 `xrtModules` 中声明。

## 范例和验证

- XLang 对照范例：`release/examples/extlibs/pkcs.xl`
- C 对照范例：`release/extlibs/pkcs/examples/pkcs_c.c`
- XLang 边界：`release/extlibs/pkcs/test/boundaries.xl` 与 `test/cases/extlib_pkcs.xl`
- C 边界：`release/extlibs/pkcs/test/test_pkcs.c`

C 范例可直接 `xl run` 单文件运行；仅该路径在 TCC 下编入包内原生实现，
普通 C 构建仍按 manifest 分离链接。双平台门禁核对两条 C 路径与 XLang
范例的输出，Linux 另由 OpenSSL 独立解析生成的 DER。

双语范例覆盖全部 16 个公开 XLang 构件：编码、解析、DER 组合、标签与子项偏移；C 范例逐次检查返回值再使用，失败路径释放已分配结果。范例中的 SET 包含两个按完整 DER 字节排序的 INTEGER；若顺序相反，`wrapSet` 报错，不会静默重排。SEQUENCE 则保留调用者给出的子项顺序。

在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/pkcs/test/check.ps1 -SkipAot`，执行严格 GCC、C/XLang 边界与双语范例对照。Linux 使用 `bash release/extlibs/pkcs/test/check.sh`，同次运行严格 GCC/ASan/UBSan C 测试、中央与包内 XLang 边界，并逐字节核对 C/XLang 范例输出。Linux 门禁还需 `openssl` 和 `xxd`，以 OpenSSL 独立解析范例生成的算法标识、INTEGER 和 OCTET STRING，核对标签与值。2026-10-17 增强后的 Linux 门禁通过。
2026-10-16 将包内 XLang `test/boundaries.xl` 登记到 `coverage.tests`；从
`demo6` 工作目录单独运行通过。本轮未重跑 C/Linux/AOT 门禁。

2026-10-14 追加 XLang 的 DER、OID 与双参数 HEX 文本内嵌 NUL 负例；先复现有效前缀被误接受，再验证统一拒绝。包内 C ABI 不变。

同日补 `wrapSequence`/`wrapSet` 构造阶段的分配故障注入：内存失败保留 `XERR_MEMORY`，只有 DER 内容不规范才报告 `XERR_VALUE`；故障清除后可继续调用。`bash release/extlibs/pkcs/test/check.sh` 在 Linux 运行本包严格 GCC 与 ASan/UBSan C 门禁，本轮通过。

不加 `-SkipAot` 的 Windows 门禁还会构建并运行 XLang 边界与范例；当前构建在共享主线缺符号 `xl6ValueDataSnapshotV1` 处停止，**AOT 尚未通过验收**。修复主线链接依赖后应重跑完整门禁。阻断记录见 `dev/extlib_blockers/text_snapshot_link.md`。
2026-10-17 修复公开头在未预先启用 XRT value 模块时无法独立包含的问题；
Windows/Linux 严格 C 门禁均固定无模块和显式模块两种编译方式，C/XLang
边界、双语范例及 Linux ASan/UBSan 包级门禁复跑通过。AOT 阻断不变。

2026-10-18：Linux 门禁新增 OpenSSL ASN.1 独立解析，检查双语范例生成的
AlgorithmIdentifier、INTEGER 与 OCTET STRING 的结构和值；完整门禁通过。
这是 DER 构件互操作检查，不代表完整 PKCS 协议或 AOT 已验收。

2026-10-18 补齐双语范例中的 `getTag`、`nextOffset`、`wrapSet`、`encodeBitString`，并修正 C 范例的失败检查顺序；包内 C/XLang 正例覆盖对应构件。Windows `-SkipAot` 和 Linux C ASan/UBSan、C/XLang 同输出及 OpenSSL 门禁通过；共享 AOT 阻断不变。

同日复核当前完整 Windows 门禁，前八项通过，随后边界 AOT 构建仍因缺少 `xl6ValueDataSnapshotV1` 停止。Linux OpenSSL 独立解析已扩展到双语示例中的 SET 和 BIT STRING；不以 C/XLang 一致性代替格式正确性。

2026-10-19：C/XLang 同标签双 INTEGER 的 SET 排序正反例通过；SEQUENCE 同序列按原顺序保留。双语范例改为展示两元素规范 SET，Windows 严格 C/XL 门禁与 Linux C ASan/UBSan、OpenSSL 独立解析及双语对照通过；完整 AOT 共享阻断未变。

2026-10-21：`nextOffset` 补 C/XLang 双侧负例：目标子项有效但尾项截断、或尾项 SET 不规范时均拒绝；随后有效输入仍返回正确偏移并清除旧错误。Windows `-SkipAot` 严格 C/XLang 与双语范例、Linux C ASan/UBSan、OpenSSL 互通及双语门禁通过。实现无需改动；完整 AOT 共享缺符号阻断仍在。

2026-10-22：C 端 `encodeOid`、`decodeOid`、`getContent` 和 `concat` 新增逐点分配故障扫描，核对内存错误、成功重试及活动分配数/字节数回到基线。Windows 严格 C/XLang 与双语范例、Linux C ASan/UBSan、OpenSSL 互通门禁通过；未发现包内泄漏，完整 AOT 仍待共享缺符号修复。
