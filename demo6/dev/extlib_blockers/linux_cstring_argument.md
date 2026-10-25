# Linux 发布版的 string -> cstring 实参边界

2026-10-25，`release/xl` 仍是 10 月 6 日的 Linux 发布物；当前包源码
使用 `cstring` 形参。运行 `bash release/extlibs/zstd/test/check.sh` 时，
C 严格编译、边界、文件和分配故障的 ASan/UBSan 测试均通过，随后
XLang 导入在 `src/zstd.xl:105` 停止：

```text
[sem] 函数调用实参类型和参数类型不匹配: target cstring, value string
```

最小复现是同目录的 `linux_cstring_argument.xl`。从 `demo6` 目录分别运行：

```sh
release/xl check dev/extlib_blockers/linux_cstring_argument.xl
```

```powershell
release\xl.exe check dev/extlib_blockers/linux_cstring_argument.xl
```

最小复现的 Windows 结果为 `ok`。旧 Linux 发布版则同时报告
`target cstring, value string`、`未知类型: cstring` 和第一阶段 C ABI
extern 参数限制。这说明缺的是新类型及其转换的整体编译器版本，
而非 `zstd` 特有的函数签名。

当前 Windows `release/xl.exe` 也能通过 `zstd` 的同一调用位置检查；
这不足以证明 Linux ABI 正确，只说明两个发布编译器的语言边界不一致。
修复应在主线同步 Linux 源码清单、重建 Linux 发布物后，重新运行
最小复现、`zstd` 完整包门禁和独立 EXE；不要在 `zstd` 中增加
`string` 形参兼容重载或复制包装。当前 Linux C 通过不能算作整包签收。
