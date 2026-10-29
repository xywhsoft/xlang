# Linux 发布构建脚本与 X509/TLS 源文件清单漂移

状态：blocked-upstream。扩展库任务不修改主线构建脚本或运行时。

2026-10-25 复验：在 WSL Ubuntu x86_64 使用新的独占构建目录，
临时 LF 副本执行当前 `build_linux.sh`。四个 shipping profile 均报告
852/852，TCC、XRT 与运行版/编译版 VFS 资源生成成功；`libxl.so`
仍在链接阶段因下述 X509/TLS ownership-domain、路径和证书函数缺符号失败。
原脚本直接运行仍在第 2 行因 CRLF 失败。临时脚本已清理，主线文件未改；
`release/xl`、`libxlc.so` 仍是 10 月 7 日旧产物，不能用来签收当前
扩展库的 Linux XLang/JIT/AOT 路径。

## 复现与观察

2026-10-24，WSL Ubuntu x86_64；`release/xl` 和 `libxlc.so` 仍是
10 月 7 日的旧发布物。运行 `bash build_linux.sh` 在第 2 行即因 CRLF
报 `set: pipefail\r: invalid option name`。使用位于 `demo6` 根目录的
临时 LF 格式副本运行相同脚本，TCC、XRT、两组 VFS 资源生成成功，
`libxl.so` 链接失败，涉及大量未定义符号，例如：

```text
xl6OwnershipDomainX509StoreV1Get
xl6OwnershipDomainX509StoreSnapshotV1Get
xl6OwnershipDomainTlsServerContextV1Get
xl6OwnershipDomainTlsSessionV1Get
xl6X509PathSnapshotV1
xl6X509PathCallBeginV1
xl6X509CertificatesCaptureV1
```

`build_linux.sh` 的 `RUNTIME_SOURCES` 包含部分 X509 使用方，但没有列入
`xl6_x509_path_snapshot.c`、`xl6_x509_path_operation.c`、
`xl6_x509_certificate_inputs.c`、`xl6_x509_store_domain.c`、
`xl6_x509_store_snapshot_domain.c` 等新实现。Windows 的
`tools/runtime_dll.rsp.in` 和 `tools/compiler_dll.rsp.in` 已列入其中至少
前述三个实现。构建开始时的标准库 shipping 检查仍报告四个 profile
完整，说明该检查不足以保证 Linux 链接闭包。

## 建议与验收

主线应将 `build_linux.sh` 规范为 LF，并把新增运行时单元同步到 Linux
运行版/编译版源文件清单；最好让跨平台清单由同一权威来源生成或校验，
避免各自手工扩张。随后在干净 WSL 环境执行 `bash build_linux.sh`，
确认三个 Linux 发布物更新且能启动，再运行
`bash tools/check_extlib_blake3.sh`。不应仅以旧 `release/xl` 的
`未知类型: cbytes` 判定 BLAKE3 包不兼容；它尚未包含当前语言能力。
