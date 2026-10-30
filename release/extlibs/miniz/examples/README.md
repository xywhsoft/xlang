# miniz 双语范例

从仓库根目录执行；Linux 将 `xl.exe` 换成 `xl`。

| 范例 | 功能与固定输出 |
| --- | --- |
| `basic.c` / `release/examples/extlibs/miniz_basic.xl` | 单条标准 zlib 内存往返，输出 `hello miniz`。 |
| `streaming_c.c` / `streaming.xl` | 分段 update、及时 drain、finish 尾部收集、解压与计数，输出 `hello stream` 和 `12 / 12` 两行。 |
| `file_transaction_c.c` / `file_transaction.xl` | 独占临时目录、UTF-8 文件名、取消/回调错误保护旧目标、立即重试。三行输出见 `file_transaction.expected.txt`。 |
| `release/examples/extlibs/miniz.xl` | 一次性/对象/文件、限额与错误的综合范例。 |

```powershell
./release/xl.exe run release/extlibs/miniz/examples/basic.c
./release/xl.exe run release/examples/extlibs/miniz_basic.xl
./release/xl.exe run release/extlibs/miniz/examples/streaming_c.c
./release/xl.exe run release/extlibs/miniz/examples/streaming.xl
./release/xl.exe run release/extlibs/miniz/examples/file_transaction_c.c
./release/xl.exe run release/extlibs/miniz/examples/file_transaction.xl
```

这些源文件也可直接 `xl build <源码> -o <目标>`。GCC 独立宿主与
按清单分离编译见 `../test/check.ps1` / `check.sh`；TCC 单文件分支
只包含现有后端，不另写一套实现。C 持有的 Buffer、Reader/Writer、
codec 和 callable 都由示例释放；文件例创建并清理自己的目录。

文件 C 例使用已有 native ABI 的 XRT callable（三个 uint64 参数、
bool 返回），不是普通 C 回调；普通 C Reader/Writer 流接口直接用
`xrtprogressproc`，见 streaming 例和 C 测试。

普通例子不能代替故障注入。打开态 96 组双故障回归已通过，Windows
关闭态回滚 OOM 与全局对象 M2 仍阻断完整签收，详包内审查报告。
