# 扩展库 C 输入与拥有型文本优化

2026-10-24，基于 214 个扩展包的静态清单实施。性能范围是消除桥接层的整段输入复制和不必要的结果复制；解析结果、输出构造、持久对象快照仍遵守各自所有权协议。

## 静态输入

| 扩展包 | 调整 |
| --- | --- |
| terraform_state | parse/valid 使用 xstrview，保留精确长度，避免 string → var |
| redis | decode/isComplete 使用文本 span，增加 bytes 入口；命令适配当前 TcpStream send/wait/recv |
| bson | decode/decodeAll/concat/toHex/docLength 增加静态 bytes 重载，保留 var 重载 |
| gettext | parseMo 增加静态 bytes 重载，目录内容仍拥有自己的存储 |
| siphash128 | 一次性哈希、密钥、增量更新直接借用 span，密钥状态内必要的 16 字节复制保留 |
| blake2/blake3 | keyed constructor 使用 bytes span，保留核心长度校验与原 C ABI |

`cstring/cbytes` 的特化现覆盖 `export func`。生成的特化函数为私有函数，公开导出仍使用原来的普通值 ABI。SipHasher 的二进制更新直接绑定带错误通道的原生 span 方法，避免先物化普通 bytes。

验证：`test/cases/xlir_extlib_c_owned_inputs.xl` 通过当前源码编译器的 JIT 和独立 EXE；EXE 保留的 `module.c` 中没有 `xl6CBytesCopyV1`、`xl6CStringCopyV1` 或 `xl6CStringBoxV1` 的隐式调用。测试覆盖 C 分配器生命周期、UTF-8、二进制 NUL、动态兼容入口、一次性与增量结果一致性。`xlir_c_owned_calls.xl` 的导出函数重载、默认参数和结果转发通过生成 C 的独立程序验证。

## 文本输入与输出

以下 7 个包去掉 9 处“创建 Buffer → append(text) → 再调用原生函数”的中间复制。

| 扩展包 | 入口 | 保留的必要所有权 |
| --- | --- | --- |
| zstd | compress(string)、Compressor.update(string) | 压缩输出及压缩器状态 |
| sharedmem | writeText | 写入共享内存本身的复制 |
| xhttp.body | appendText、prependText | 不可变 Body 的片段快照 |
| xhttp.client | Request.setBody(string) | Request 自有 Body 快照 |
| csv | parse(string) | Reader 自有输入快照 |
| xml.sax | reader(string) | Parser 自有输入快照 |
| zip | Archive.add(name, string) | 延迟提交 ZIP 条目的内容快照 |

`xhttp.protocol` 的 Headers、Cookies、Set-Cookie、MediaType 构建器，以及 `xhttp` 的 QueryParams 和 form encoder，增加直接返回拥有型文本的原生入口。XLang 使用 `takeCstr` 接管该分配；原来的 C Buffer 返回入口仍保留，并复用构建器已知的长度，不增加一次 strlen 扫描。原生分配计数测试确认直接文本路径比 Buffer 路径少一次分配，释放后恢复原有存活计数。

这些协议构建器保证输出不含内嵌 NUL。form decoder、任意二进制正文和其他长度敏感输出继续使用带长度的接口。`cstring.take` 本身仍以终止 NUL 确定长度，不用于接管包含内嵌 NUL 的缓冲区。

## 编译兼容与边界

- 原生析构增加 void ABI 适配入口，原有 C 返回签名保留。
- CSV 字典层的原生资源字段使用 nullable 构造期存储；行循环先取得一次稳定 Reader，避免逐字段重复检查。
- CookiesIter、MultipartWriter、FormDataEncoded 对构造期 nullable 存储提供非空只读属性。对应的 `cookies`、`boundary`、`body` 视图通过属性读取；重建对象使用构造函数。
- multipart 文本快照改用当前 Buffer.append；HTTP 回调失败使用 XRT 的统一错误通道。
- 分拆 HTTP 包会重新计算累积模块特性；公共静态错误助手的 header guard 不再阻止另一个包激活自身模块。正反包含顺序均有编译检查。

完整的 `xhttp.client` 语言门面仍包含旧 `TcpStream(point)`、TLS/NetEngine 裸句柄接口，当前主线标准库已收窄这些入口。本轮测试使用真实 Request C 组件和现有依赖包验证 setBody 输入快照；不把组件测试当作完整 HTTP/TLS 传输验收。

本轮没有为全部 Buffer API 批量增加 bytes 重载。异步队列、借用型 multipart、SQLite TRANSIENT、字体持有输入、网络 Ref/Take 租约等，继续保留各自的快照和所有权协议；外部 DLL、未知释放器和不能证明归属的指针不做接管。没有修改 XRT 值布局或普通 string/bytes 的读取分支。

## 可复现验证

先构建当前源码驱动，再执行：

```powershell
pwsh -NoProfile -File tools/build_xlir_public_source_driver.ps1 -Directory build/c-owned/extlib-public
pwsh -NoProfile -File tools/check_extlib_c_owned.ps1 -PublicDirectory build/c-owned/extlib-public -Directory build/c-owned/extlib-acceptance
```

验收脚本运行 4 个 JIT/独立 EXE 场景、CSV 字典与边界、正反 HTTP 头包含顺序及 10 个原生测试，检查静态输入生成代码无隐式 C 载荷物化，并记录源码、编译器、对象文件的 SHA256。验收目录必须为新的目录。测试使用 `xlir_` 前缀，避免让仍依赖旧发布编译器的通用扩展库套件误执行这些新编译器场景。
