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

验证：`test/cases/extlib_c_owned_inputs.xl` 通过当前源码编译器的 JIT 和独立 EXE；EXE 保留的 `module.c` 中没有 `xl6CBytesCopyV1`、`xl6CStringCopyV1` 或 `xl6CStringBoxV1` 的隐式调用。测试覆盖 C 分配器生命周期、UTF-8、二进制 NUL、动态兼容入口、一次性与增量结果一致性。`xlir_c_owned_calls.xl` 的导出函数重载、默认参数和结果转发通过生成 C 的独立程序验证。
