# 扩展库字符串返回审计（2026-10-24）

本轮扫描 `release/extlibs` 下全部 214 份包清单及其 XLang/C 返回包装，并对有复制嫌疑的出口核对所有权。以下数量是源码词法候选，类别会重叠，不能当作 214 包逐项运行验收：

| 形态 | 涉及包数 | 结论 |
| --- | ---: | --- |
| `__xl_runtime.takeCstr` | 61 | 对 XRT 拥有、零结尾的分配已直接转交载荷；改成 `cstring.take` 反而多一个 owner 记录。 |
| 源码 `return ...text()` | 22 | 逐类核对 NUL、Buffer 来源和对象寿命；不能整批替换。 |
| `extern ... as string` | 158 | 大多是 C 静态/对象借用文本，语言侧一次复制是必要的；需额外识别 C 已先复制到临时区或结果槽的情况。 |

## 已处理

- `aho_corasick`、`xregex`：既有无 NUL 快路转交原生 Buffer；有 NUL 时保留长度感知复制。
- `xhttp`、`xhttp.protocol`、`xhttp.client`：通用文本桥、错误快照和确定为文本的查询/属性返回改为 Buffer 数据转交；含 NUL 的结果仍走 `Buffer.text()`，扩容失败则按原生内存错误传播。正文、解码后的任意字节不走 C 字符串快路。
- `sqlite`：列文本、列名、连接/语句错误和行号文本新增一次复制的拥有型 C 入口，XLang 直接接管；既有 C 借用入口不变。含 NUL 的列继续要求字节接口。
- `volc.core`：离线摘要、规范查询、JSON 编码和签名预览从线程结果槽取走分配；静态失败文本复制一次。HTTP 响应体仍不按零结尾文本接管。

## 保留原路径

- `brotli`、`bz2`、`gzip`、`lz4`、`lzma`、`miniz`、`snappy`、`zstd` 的文本便捷函数从解压后的任意字节 Buffer 转换；数据可以包含 NUL，当前 `takeCstr` 不能保留长度。
- `serial`、`xhttp.body`、`xmail`、`xssh`、`xws` 的原始数据、正文、MIME 或协议载荷也可能含 NUL；不能仅按返回类型名称将 Buffer 变成 C 字符串。`xmail` 的协议状态短文本可另行单独优化，不能连同正文批量修改。
- `ui.auto` 的 `element.text()` 是语言对象访问；`xbot.agent` 的 `buf.text()` 是内容聚合，不是 C 返回值重复包装。
- 静态表、SQLite/其他数据库内部视图、DLL 内部缓存等借用指针不能由 `takeCstr` 或 `cstring.take` 释放。`onnxruntime`、`opencv`、`rapidocr` 外部桥未提供完整分配/释放合同的部分，仅保持调用通路，不推断可接管所有权。

## 后续边界

- `xhttp.form_data`、`xhttp.multipart` 的文本与二进制共用部分桥；须逐个确认字段长度和借用生命周期。它们的 C 源码正在其他工作中修改，本轮不叠加改动。
- `xhttp.client` 的文本桥 C 测试通过，但 XLang 检查先被依赖 `xhttp.multipart` 的 `Buffer.appendBytes` 语义错误阻断；`volc.core` 同样被该导入链阻断。
- SQLite 的 Windows C 与 Linux ASan/UBSan 门禁通过，现有发布版 XLang 运行仍被 `sqlite.xl:550` 的 XLIR ABI 错误阻断。`xhttp` URL 用例在本轮快路关闭时也同样报 `ValueError`；查询用例的 Windows/WSL 路径通过。
- 若语言层提供“指针 + 明确长度 + 释放器”的拥有型字符串接管，再评估含 NUL 的文本和部分解码结果。当前 `cstring.take` 通过 `strlen` 建立长度，不能冒充该能力。

每次变更只为已经确定的所有权合同做转交，不新增旧语法兼容或隐藏释放器。审计清单不代替包级 C、XLang、AOT 与跨平台发布门禁。
