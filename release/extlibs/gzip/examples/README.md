# Gzip 双语范例

从仓库根目录运行：

```powershell
./release/xl.exe run release/extlibs/gzip/examples/basic.c
./release/xl.exe run release/examples/extlibs/gzip_basic.xl
./release/xl.exe run release/extlibs/gzip/examples/file_transaction.xl
./release/xl.exe run release/extlibs/gzip/examples/file_transaction_c.c
./release/xl.exe build release/extlibs/gzip/examples/file_transaction.xl -o file_transaction.exe
./file_transaction.exe
```

Linux 将 `./release/xl.exe` 改为 `./release/xl`，可执行文件不加 `.exe`。
C 源同样可 build；基础例使用相同 UTF-8/二进制输入，输出匹配
`release/examples/extlibs/gzip_basic.expected.txt`。

文件例在独占临时目录使用 UTF-8 文件名。布尔取消和业务异常都
保留旧目标，随后用正常操作重试，结束时释放资源并清理目录。
两种语言匹配 `file_transaction.expected.txt`：

```text
cancelled: true
callback error: true
restored: hello gzip
```

C 沿用 `xgzip_*_file` 的正式 XRT callable ABI，回调返回布尔值，
业务失败通过错误槽及false返回；不是普通C `xrtprogressproc`。
普通C回调由核心 `xrtGzipCompressStream/DecompressStream` 提供。
首次包含XRT前选择所需模块，callable由xruntime启用相应依赖；
程序只链接一份运行库。回调value、callable及返回bool分别按所有权释放。

Windows/Linux 的 GCC C、TCC C JIT/AOT、XLang JIT/AOT同输出。
这些函数内普通用例不替代全局对象AOT退出测试，也不覆盖提交失败
叠加OOM的关闭态回滚；详包内 `docs/io-file-audit-2026-10-31.md`。
