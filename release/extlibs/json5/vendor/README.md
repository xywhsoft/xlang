# libpdjson5 来源与本地补丁

声明的上游为 [boris-kolpackov/libpdjson5](https://github.com/boris-kolpackov/libpdjson5)，
版本 `ed602821f2929930e5179cd9240d468abf210d60`，许可见
[UNLICENSE](UNLICENSE)。正常构建直接编译本目录源码，不访问网络。

## 本地差异

本包不是未经修改的上游快照。此前已加入 Unicode IdentifierName 键、
Unicode 空白、代理对和数字处理等定点修复；Unicode 17.0 属性表由
`gen_unicode_table.mjs` 生成，普通构建无需 Node。表中数据的许可见
[Unicode License](https://www.unicode.org/license.txt)。重新生成时脚本
会拒绝 Unicode 版本不符的 Node，避免无意改变可接受字符集合。

2026-10-30 的追加修复只调整 `pdjson5.c`：

- 抽取完整 UTF-8 字符的读取/校验，字符串与注释复用栈上四字节缓冲。
- JSON5 的非 ASCII identity escape 保留完整字符；实际 LS/PS
  转义续行删除字符，而原始 LS/PS 字符串内容保留，两者都推进行号。
- ASCII 续行正确计数 LF、CR、CRLF；注释识别 LF、CR、CRLF、LS、PS，
  单行注释在它们处结束，多行注释继续读取，并修正非 ASCII 列号。
- 严格 JSON 不因此接受 identity escape。未新增公开 ABI、分配器、
  限额、额外整文档扫描或临时字符串；未做性能基准。

本包公开入口只使用内存 buffer，通用 vendored 流 API 不是本包对外
承诺的新功能。此次不是所有源码位置 API 的全面改写：例如字符串/
注释外的单独 CR 位置计数不在本轮修复和签收范围内。

以下为本次现场文件 SHA256，不将修复前文件冒充原始上游；换行转换
会改变文件哈希，更新时应同时审查实际差异和重新执行门禁。

| 文件/快照 | SHA256 |
| --- | --- |
| 修复前 `pdjson5.c`（已有本地补丁） | `b057b05d92329de41510b9680966433fec0de329e19bbf6a6c6910824ea491b3` |
| 本轮修复后 `pdjson5.c` | `c9df7b5e9c05ecd9d6436a900768d9305f307dc45fbc9c9a112962694fd86eaa` |
| `pdjson5.h`（未改） | `36a59810a848880c82a92bbb1c277e8ba5390854cb392e3cca6a42bc7b5069b2` |
| `pdjson5_unicode.inc`（未改） | `06875a947854fa6c9762e22f9775760d143103967345972333955a7484a87120` |
| `UNLICENSE`（未改） | `640514163b17f977adc997cb16f51871122cfb0555ebad1a3f01e167b7ba8857` |

## 更新验收

在仓库根目录运行包内 `test/check.ps1` 或 `test/check.sh`，保持
`test_unicode_escapes.c` 的字符串/注释、诊断、OOM、严格 JSON 对照，
以及原数字、深度、分配故障测试。可选官方 JSON5 2.2.3 对照不能代替
这些本地资源与所有权断言。M10 正例不得删除或把失败记作成功；当前
完整状态见[包 README](../README.md)。
