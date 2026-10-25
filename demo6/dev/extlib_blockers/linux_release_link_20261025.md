# 2026-10-25 Linux 发布构建阻断

## 状态：已修复并完成标准构建

2026-10-25 主线增加 shell 脚本 LF 规则，并让 Linux 与 Windows 共用
经过校验的物理源码清单。WSL 标准 `bash build_linux.sh` 已完整生成
`libxl.so`、`libxlc.so` 和 `xl`；hello、资源桥接 JIT/AOT 以及 GCC O2
资源故障测试通过。构建与验收日志位于实现工作树的
`build/c-owned/linux-shared-sources-20261025-3.log` 和
`build/c-owned/foreign-bridges-linux-acceptance-1.log`。
以下保留修复前的复现；各扩展包仍须用新发布物重跑自己的发布门禁。

普通扩展库的 Linux 发布版复验需要当前源码构建的 `release/xl`。
本任务不修改 xlang 主线；以下是 WSL Ubuntu x86_64/glibc 上的复现。

## 复现

从仓库根目录运行：

```powershell
wsl bash -lc 'cd /mnt/d/GIT/x-lang/demo6 && bash build_linux.sh'
```

工作树中的 `build_linux.sh` 为 CRLF（`git ls-files --eol` 显示
`i/lf w/crlf`），在第 2 行报 `set: pipefail\r: invalid option name`。
`build.sh` 是 LF，但它也会调用同一脚本，不能绕开该问题。

为了只验证下一阶段而不修改主线文件，本次将脚本流在执行时转为 LF，
并显式设置脚本根目录：

```powershell
wsl bash -lc 'cd /mnt/d/GIT/x-lang/demo6 && bash <(sed -e "s/\r$//" -e "s|^ROOT=.*|ROOT=/mnt/d/GIT/x-lang/demo6|" build_linux.sh)'
```

XRT 快照检查、Linux TCC、VFS 资源生成均通过；`libxl.so` 链接失败，
报未定义的 `xl6OwnershipDomainX509StoreV1Get`、
`xl6OwnershipDomainTlsIdentityV1Get`、`xl6X509PathSnapshotV1`、
`xl6X509CertificatesCaptureV1` 等一批 TLS/X509 符号。
例如后两者分别定义在 `src/runtime/xl6_x509_path_snapshot.c` 和
`src/runtime/xl6_x509_certificate_inputs.c`，但这两个文件不在
`build_linux.sh` 的 `RUNTIME_SOURCES` 中。当前运行时目录有 1161 个
`.c`，该脚本显式列出其中 1039 个；未列出的 122 个不能一概认定
都应编译，但已出现真实的链接缺口。

## 建议

1. 为 Linux shell 脚本固定工作树 LF（例如受控的 `.gitattributes`
   规则），并从 Windows 工作树在 WSL 直接执行一次构建门禁。
2. 同步 Linux 运行时源清单与实际符号提供者；优先共用可验证的
   源清单/生成规则，避免 Windows 和 Linux 独立维护两份长列表。
3. 增加当前源码的 Linux `libxl.so`/`libxlc.so` 链接及最小
   `xl run`、`xl build` 验收，而不只检查资源/头文件快照。

本次构建退出码为 1，不将既有 `release/xl` 视作新发布物；`gzip` 等
此前受旧 Linux 二进制阻断的包仍待主线修复后重跑。扩展库侧不通过
额外包装或缺省符号来掩盖链接错误。
