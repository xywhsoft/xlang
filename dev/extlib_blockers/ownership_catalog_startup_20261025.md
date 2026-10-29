# Windows 发布版运行入口在 ownership catalog 初始化失败

## 后续复验

2026-10-25 再次从当前工作区运行 `build.bat`，完整生成
`xl.dll`、`xlc.dll` 和 `xl.exe`。新发布物在 `release` 执行
`./xl.exe run examples/hello.xl` 退出 0；
`./xl.exe run examples/extlibs/toml.xl` 也正常输出并退出 0。
因此本页记录的 ownership catalog 启动故障仅适用于下述旧发布物，
不再作为当前 Windows 扩展库运行的共同阻断。它不证明全部包已通过：
TOML 包级边界仍在 `stringify(var)` 报
`TypeError: TOML document root must be a dict`，属于另记的
foreign `var` 容器投影问题。Linux 新发布构建仍受
`linux_release_link_20261025.md` 中的链接缺符号阻断。

2026-10-25 在 `demo6` 执行 `build.bat` 成功，得到新的
`release/xl.dll`、`release/xlc.dll` 和 `release/xl.exe`。构建前
`tools/check_xrt_snapshot.ps1` 通过；能力矩阵按当前源码同步后再构建。

最小复现（从 `release` 执行）：

```powershell
.\xl.exe check examples\hello.xl
# ok
.\xl.exe run examples\hello.xl
# cannot initialize module ownership policy catalog (exit 1)
```

`extlibs/json5/examples/json5.xl` 和
`extlibs/terraform_state/test/literal_state.xl` 的 `run` 也在同一位置失败。
这发生在用户脚本执行前，与扩展库实现无关。当前 `xl.exe` SHA-256：
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
源码工作区含并行任务的未提交改动，不能仅用 `HEAD` 还原该二进制。

`ownership_catalog_probe.c` 通过公开 `xlc.dll` / XRT API 缩小了错误范围：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Idemo6 -Irelease/include -Ilib/xrt dev/extlib_blockers/ownership_catalog_probe.c release/xlc.dll -o release/ownership_catalog_probe.exe
cd release
.\ownership_catalog_probe.exe
# getter not exported: 68/261 xl6OwnershipDomainTaskPoolExitV1Get
# getter not exported: 218/261 xl6OwnershipDomainTlsInfoV1Get
# getter not exported: 219/261 xl6OwnershipDomainTlsServerStatsV1Get
# exported domains accepted: 258/261
# xlRunScriptString: cannot initialize module ownership policy catalog
# xlRunScriptString: kind=1 domain=xrt.core operation=validate message=invalid argument
```

`xlCreateContext` 成功；失败发生在 `xlRunScriptString` 的 module 创建阶段。
XRT 错误类别是 `XERR_ARGUMENT`，并非资源不足。`xl.exe` 的统一报错隐藏了
这一层诊断。探针从源码注册表依次取公开 getter，258 个可导出的前缀
全部通过；另有 3 个 getter 只在 DLL 内部可用，无法在外部重组完整
261 项 catalog。**不能据此断言三者中哪一个有错**，也不能排除它们与
其他 domain 的交互。`xl.dll` 的公开运行入口同样报这一 XRT 错误。
探针只读取公开 API，不触碰主线实现。

调用点在 `src/api/xl_api.c` 的 `xl6OwnershipCatalogCreateV1` 返回空值后，
统一设置了上述错误。建议由主线维护者先查出首次失败的 domain/Load
及其 `validate` 参数，再修复策略注册或 ABI 不一致；不要在扩展库侧跳过
catalog。发布构建门禁应增加独立的 `xl run examples/hello.xl`，避免
只有 `check` 和构建成功却无法执行任何 XLang 程序。
