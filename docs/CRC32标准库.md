# CRC32 标准库

默认命名空间 `crc32` 复用 XRT 的独立 `crc32` 模块，提供 IEEE
CRC-32/ISO-HDLC 校验和，不是 CRC32C，也不能用于认证或防篡改。
无须 `import`，不创建 Hasher 对象。

## 接口

| 调用 | 返回 |
| --- | --- |
| `crc32.checksum(string data, int size = -1)` | `uint32` |
| `crc32.checksum(bytes data, int size = -1)` | `uint32` |
| `crc32.update(uint32 previous, string data, int size = -1)` | `uint32` |
| `crc32.update(uint32 previous, bytes data, int size = -1)` | `uint32` |
| `crc32.checksumBuffer(Buffer data, int size = -1)` | `uint32` |
| `crc32.updateBuffer(uint32 previous, Buffer data, int size = -1)` | `uint32` |

`size` 以字节计。`-1` 读取完整输入；非负值截断到实际输入长度；小于 `-1`
抛出 `RangeError`。字符串按 UTF-8 原始字节计算，内嵌 NUL 不截断。
Buffer 读取当前内容，不修改数据；调用期间不得并发修改 Buffer。

```xl
uint32 expected = crc32.checksum("123456789")
assert expected == uint32(0xCBF43926)
uint32 state = crc32.update(uint32(0), b"1234")
state = crc32.update(state, "56789")
assert state == expected
assert crc32.checksum("a\0b") == crc32.checksum(b"a\0b")
```

增量接口传递已最终化的 CRC，首次传零，不由开发者取反。空输入结果为零，
空更新保持 `previous`，结果没有符号扩展。

## 编译与体积

公开声明经已有 XLIR 原生 ABI 合同调用三个按需驻留适配器；适配器只做
输入及长度检查，再调用 `xrtCrc32Update`。Cgen 不生成 CRC 算法。
字符串、bytes 和 Buffer 分为独立原生组件；字符串适配器不额外选择
bytes、Buffer 或压缩适配组件。断言格式化、对象拥有图等语言基础设施
原有的动态值与 Buffer 依赖不因此消失。CRC32 算法与 gzip、VFS pack 共享一份实现和
64 字节只读表，不新增 8 KiB slicing-by-8 表。

字符串和 bytes 入口声明为 `pure + nonblocking`，但尺寸检查会抛错，
不能标记 `no_raise`。Buffer 可变，因此不声明 `pure`。

现有同名扩展库及其文件和 Hasher API 不在本次迁移范围内。默认命名空间
已由标准库占用，新代码不应再显式导入旧 `crc32` 扩展库。

## 决策范围

本次只批准并实现 CRC32 独立和语言接入，不将 X06 的 Hex、Base64、Percent、
Inflate、Deflate 等其余公开面标为已决策或已开发。对象池仍最后处理。

## 验证范围

Windows 28 阶段、Linux 30 阶段专项通过：重建产品的 JIT、模块运行、两种
运行时库的原生 ABI、独立程序搬移运行、三种错误合同拒绝，以及保存的
原生实现 O0/O2 严格编译。与断言基线相比，只新增 CRC32 特性；Linux
实际组件计划只选择 `xl6_crc32_text.c`，不选择另外两个 CRC32 适配器。
Linux O0 对象中的 CRC32 查表大小为 64 字节。

构建使用冻结的已验收 B350 源码加本次 CRC32 增量，不纳入工作区其他
IDE 或扩展库改动，也不替换仓库已有的发布二进制。这是 CRC32 专项验收，
不是一次新的全仓 RC0 验收。可通过 `tools/check_stdlib_crc32.py` 对重建的
候选运行同一专项；每次提供新的输出目录，保留旧报告。
