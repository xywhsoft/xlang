# 原生源依赖与 C 入口合同

这组夹具验证显式混编 C、声明模块的扩展包和纯 xlang 程序使用同一套运行库
选择规则。它不是独立扩展包，也不会被安装到 `release/extlibs`。

## 覆盖范围

| 夹具 | 目的 |
| --- | --- |
| `direct.xl` + `../sdk_feature_probe/sdk_features_native.c` | 显式追加 C 源，JIT 与独立 EXE 均可调用完整 XRT/xruntime |
| `multiple.xl` + 两个 C 文件 | 多个原生源共享一致的功能选择 |
| `package_and_direct.xl` + `extra.c` | 精确模块包和无模块合同的显式 C 源同时存在 |
| `native_project.xlp` | 项目中的 `cSources` 与命令行追加 C 源语义相同 |
| `exported.xl` + `c_main.c` | C 拥有 `main`，编译器不再生成冲突的 `main` |
| `exported.xl` 生成的 DLL + 独立 GCC 消费者 | 生成头承诺的错误作用域与错误释放接口确实由 DLL 导出 |
| `missing_symbol.c` / `duplicate_main.c` | 未知符号和重复入口仍被明确拒绝，不靠链接重试补实现 |

门禁另外保留声明模块包、纯 xlang 和仅全局初始化三个对照组；它们仍采用精确
模块选择，不能因为支持混编就统一切换成完整运行库。读取生成实现是显式使用
`--save-temps` 的验收行为，不是普通 `run` 或 `build` 的落盘要求。

## C 调用与生命周期

混编时生成头由 xl 注入，独立 DLL 消费者使用 `--emit-header` 输出的公共头。
导出入口负责初始化模块。用 `xl6ForeignCallBegin/End` 获取完整成功状态，不能
把返回零或诊断为空当作调用成功；返回的错误由同一 DLL 的 `xrtErrorFree` 释放，
不在消费者里再编译一份 XRT。

`c_main.c` 覆盖成功、抛错、失败后恢复、嵌套清理作用域、无诊断失败和回调失败。
结束时检查当前模块/运行库清理结果。DLL 的 `xl_script_runtime_retire_checked`
是终止生命周期的操作，测试不在此后继续调用业务函数；这不是热卸载压力测试。

## 重跑

在 `demo6` 根目录执行，三条命令使用同一个全新证据目录。需要 GCC、当前冻结
的 `release/xl.dll` 和仓库 SDK/资源。工具不替换任何发布二进制或运行库实现头。

```powershell
powershell -NoProfile -File tools/build_xlir_public_source_driver.ps1 -Directory build/test/native-contract/compiler
powershell -NoProfile -File tools/stage_xlir_artifact_runtime_sources.ps1 -Directory build/test/native-contract/compiler
powershell -NoProfile -File tools/check_extlib_native_contract.ps1 -PublicDirectory build/test/native-contract/compiler -Directory build/test/native-contract/checks
```

门禁固定输入哈希并记录命令、输出及状态；已接入 `test/suites/12_phase1_extlibs.bat`。
当前范围是 Windows 当前源码隔离构建、外置 SDK、C/C++ 公共头、JIT/EXE/DLL。
纯内置 VFS 发布部署、Linux 共享库、多 DLL 同时加载和重复卸载仍需各自验收。
