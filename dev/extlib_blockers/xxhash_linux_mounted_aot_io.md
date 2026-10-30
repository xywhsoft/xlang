> 历史工作树记录：以下结果对应文内冻结版本，不自动代表主目录当前 0.6.0；需在当前发布物上复验。

# XXHash Linux AOT：WSL 挂载盘 I/O 超时

2026-10-28，在 `codex/extlibs-release-sync` 的 `b77daa557` 加本轮
XXHash 所有权修改上复验。Windows 发布版完整包门禁通过；Linux C
ASan/UBSan、语言 JIT、生成 C 和原有 AOT 通过，新
`release/extlibs/xxhash/test/ownership.xl` 的 AOT 在 Windows 挂载盘
触及 180 秒限制。此记录不修改 xlang 主线，也不改哈希语义来规避。

## 已验证的对照

- 从 `/mnt/c/Users/Administrator/.codex/worktrees/extlibs-release-sync/x-lang/demo6`
  执行下方构建命令，底层 Linux 返回 **124**，墙钟约 180.76 秒。
  通过 PowerShell 显式读取 `$LASTEXITCODE`，避免把包装层的 `1`
  误判为编译器主动失败；没有编译器错误诊断。
- 运行约 130 秒时，`ps` 中 `xl` 为 `D` 状态，累计 CPU 约 8 秒。
- 将相同 `release/xl`、`release/libxlc.so`、`xxhash` 包、三份中央
  用例及两份公开 XLang 范例原样复制到 WSL `/tmp`，保留目录层级，
  同一命令约 9.34 秒构建完成，运行输出 `xxhash owned text: PASS`。

```sh
timeout 180s ./release/xl build release/extlibs/xxhash/test/ownership.xl \
    -o /tmp/xlang_xxhash_mounted_owned_probe
printf 'linuxExit=%s\n' "$?"
```

证据支持挂载盘 I/O 是主要耗时来源，不支持将其当作 XXHash 算法
错误、所有权回归或编译器无诊断返回 1。尚未定位到具体文件读入、
资源加载或 TCC 子阶段，不宣称所有 WSL 慢路径均已解释。

## 包侧处理与主线建议

包级 Linux 门禁只把语言 JIT/AOT 输入复制到本次独占的原生临时
目录，C sanitizer 仍读取当前工作树；结束前回到原目录核对输入
哈希。不放宽 180 秒限额，也不改变发布包的依赖和公开能力。

主线若要优化，应为 TCC 源码/头文件读取、VFS 查询、编译和链接
分别记录耗时，在无并行构建时对比挂载盘与原生文件系统，避免仅
凭总耗时重写哈希库。相关先例见
[ZIP 挂载盘阶段超时记录](zip_linux_tcc_stage_timeout.md)。
