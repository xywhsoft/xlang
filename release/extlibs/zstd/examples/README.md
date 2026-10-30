# Zstd 双语范例

从仓库根目录运行，无外部素材，不写数据文件：

```powershell
./release/xl.exe run release/extlibs/zstd/examples/basic.xl
./release/xl.exe run release/extlibs/zstd/examples/basic_c.c
./release/xl.exe run release/extlibs/zstd/examples/bounded_stream.xl
./release/xl.exe run release/extlibs/zstd/examples/bounded_stream_c.c
./release/xl.exe build release/extlibs/zstd/examples/bounded_stream.xl -o bounded_stream.exe
./bounded_stream.exe
```

Linux 将 `./release/xl.exe` 改为 `./release/xl`，独立程序不加 `.exe`。
C 源同样可以 `build`；两种语言的 basic 都输出
`zstd round trip: hello zstd`。bounded_stream 都自行断言并输出：

```text
source bytes: 7
completed frames: 1
binary round trip: true
output limit rejected: true
zstd bounded stream: OK
```

输入分为 `one\0` 和 `two`，NUL 是真实的二进制字节。每次 `drain`
只取当前产出，可能为空；`finish` 写出帧尾，之后还需最后一次 drain。
解压器的 `finish` 校验帧完整性；示例同时检查原文和完整帧数。
输出预算1字节会产生范围错误，随后用新操作正常往返，不能把已失败
的流重新当作有效流继续使用。

C 创建的流分别 Destroy，drain 和一次性结果分别 BufferDestroy；
XLang 自动释放。输入仅借用，返回 Buffer 独立拥有。正常分段例不
注入内存故障；后端、XRT 分配故障与失败恢复专项放在 `test`。
本例在函数内演示资源使用，并不替代仍失败的全局对象 AOT 测试。

## 文件事务

```powershell
./release/xl.exe run release/extlibs/zstd/examples/file_transaction.xl
./release/xl.exe run release/extlibs/zstd/examples/file_transaction_c.c
./release/xl.exe build release/extlibs/zstd/examples/file_transaction.xl -o file_transaction.exe
./file_transaction.exe
```

两种语言在独占临时目录使用 UTF-8 文件名，取消和业务异常都保留
旧目标，随后正常重试，并在结束时清理所创建的文件及目录。输出见
`file_transaction.expected.txt`：

```text
cancelled: true
callback error: true
restored: hello zstd
```

C 范例沿用正式 `xzstd_*_file` 的 XRT callable ABI，回调返回 bool，
业务失败通过 XRT错误槽及false返回；它不是普通 `xrtprogressfn`。
独立 C 的 `xrtZstdCompressStream/DecompressStream` 支持普通进度回调，
文件范例并未添加另一套平台 IO 或事务实现。资源分别 Release/Unref。
Windows/Linux 的 GCC C、TCC C JIT/AOT、XLang JIT/AOT 同输出。
正常取消/重试不替代关闭态回滚 OOM 及顶层对象 AOT 的失败边界，
详包内 `docs/io-file-audit-2026-10-31.md`。
