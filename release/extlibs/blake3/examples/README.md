# BLAKE3 双语范例

从仓库根目录运行，不需要服务、密钥文件或额外 DLL：

```powershell
./release/xl.exe run release/extlibs/blake3/examples/basic.c
./release/xl.exe run release/examples/extlibs/blake3_basic.xl
./release/xl.exe run release/examples/extlibs/blake3.xl
./release/xl.exe build release/extlibs/blake3/examples/basic.c -o blake3_basic.exe
./blake3_basic.exe
```

Linux 将 `./release/xl.exe` 改为 `./release/xl`，独立程序名不加 `.exe`。
[basic.c](basic.c) 与 [blake3_basic.xl](../../../examples/extlibs/blake3_basic.xl)
使用同一官方向量中的字节输入、32字节密钥和用途上下文，输出与
[共同答案](../../../examples/extlibs/blake3_basic.expected.txt) 完全相同。
七行依次展示普通哈希、独立 clone、输入字节数、reset、keyed、KDF 和
跨越64字节块的65字节XOF。C 用 Destroy 释放哈希器，XLang 自动释放；
digest 不终结对象，短输出是长输出的前缀。KDF 不代替口令慢哈希。

完整教学例 `release/examples/extlibs/blake3.xl` 还包括文件哈希，使用
独占临时目录并清理。两种语言可按需要把返回的摘要编码成 hex；不用
先转为文本再把 hex 还原成二进制。

C 的 `xrtBlake3File` 借用 Reader，不负责关闭；失败不修改摘要输出。
先读取 `xrtblake3error` 和线程错误，再由调用方关闭 Reader。错误来自
Reader 或进度回调时，`iBackendCode` 是原 XRT 错误的数字码，而不是
通用的模块状态码；原线程错误对象保持不变。回调中断的模块分类仍为
CANCELLED，具体用户异常种类从线程错误读取，不混为同一层信息。

边界演示可直接运行：

```powershell
./release/xl.exe run release/extlibs/blake3/test/test_reader_cause.c
```

应输出 `blake3 reader cause: 96 cases, snapshots, no-allocation and recovery PASS`。
它包含错误码上下界、早/晚失败、空快照、首因后禁止分配、输出不变、
Reader关闭归属及立即重试检查。普通示例不会注入故障；包的私有构造
和顶层对象独立程序阻断仍需在主线解决，不能用函数内示例通过来关闭。
