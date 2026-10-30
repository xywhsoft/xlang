# LZMA 双语范例

从仓库根目录执行，所有例子无网络或第三方运行依赖。

| 范例 | 功能与输出 |
| --- | --- |
| `basic.xl` / `basic_c.c` | 一次性标准 XZ 往返，输出 `lzma round trip: hello lzma`。C 返回 Buffer 的所有权由调用方释放。 |
| `file_transaction.xl` / `file_transaction_c.c` | 独占临时目录、UTF-8 文件名、取消/回调错误保护旧目标、立即重试。三行输出见 `file_transaction.expected.txt`，成功/失败均清理自身资源。 |
| `release/examples/extlibs/lzma.xl` | 标准库综合示例：内存、分块对象、串联成员、级别、限额和文件。 |

```powershell
./release/xl.exe run release/extlibs/lzma/examples/basic.xl
./release/xl.exe run release/extlibs/lzma/examples/basic_c.c
./release/xl.exe run release/extlibs/lzma/examples/file_transaction.xl
./release/xl.exe run release/extlibs/lzma/examples/file_transaction_c.c
```

Linux 对应使用 `./release/xl`。四份源码也可直接 `xl build <源码> -o <目标>`
并运行，输出不变；C 单文件的 TCC 分支只为此包含现有后端源码，正式包
和 GCC 路径仍分离编译，不复制另一套实现。

GCC 分离构建由 `../test/check.ps1` / `check.sh` 演示并自动与 XLang
比较。基础 C 使用 `xrtLzmaCompress/Decompress`；文件 C 使用已有 native
文件 ABI，回调是 XRT callable（3 个 uint64 参数、bool 返回），不是
普通 C `xrtprogressproc`。普通回调的 C 用户可使用公开 Reader/Writer 流 API。

文件例只演示普通取消/回调错误，不是双故障 OOM 验收替代品。打开态
96 组回归已通过；Windows 关闭态回滚 OOM 与 M2 顶层 AOT 收尾仍开放，
详包内当前审查记录。不要把例子移到全局来规避生命周期检查。
