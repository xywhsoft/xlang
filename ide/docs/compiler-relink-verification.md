# 随包编译器的实际 TCC 重链接验收

2026-10-11 已从当前便携候选内的源码复制建立独立目录，修改实际 TCC 组件后冷构建并执行验证。技术重链接能力已有证据；`relinkReviewComplete`、`legalDistributionReviewComplete` 与正式发行状态继续保持 false，原有材料核对仍单独处理。

## 实际操作和结果

输入直接来自 [当前候选](../CURRENT-CANDIDATE.md) 的 `sources/xlang/`。包内清单与 1,532 份源码／资源／构建文件逐项核对，包含 1,521 份来源和 11 个根脚本。新副本没有预置 `xl.exe`、`xl.dll`、`xlc.dll` 或 build 对象目录；2,085 份已核对的扩展运行文件另行保留，本次没有重建它们。

测试只在副本的 `lib/tcc/libtcc.c` 末尾增加一个返回固定整数的私有测试函数，不修改原包。随后实际执行 `cmd /d /c build.bat`，冷构建退出 0，用时 326.046 秒。新 `xl.dll` 与 `xlc.dll` 的摘要均改变。

两个独立进程分别加载准确路径的新 DLL，查询并执行 `tcc_xlang_relink_probe_v1`，实际返回 `0x584C4931`。加载模块路径、输出摘要、符号和值均记录。这证明修改过的 TCC 组件已进入两份实际 DLL；检查同时查询真实标量目标，保留其原有值。

原包和重建编译器随后对同一中文／空格／emoji 路径项目执行真实 `c-bridge --json --c-environment`。标量目标、位域默认模式与生成的 ABI 声明逐项／逐字节一致。重建编译器混编 C 与 xlang，实际运行 `vsnprintf` 函数指针和 xlang 导出加法并返回 0。

最终调用当前精确构建的原生 IDE C 会话夹具和包内默认 clangd，对重建编译器完成 CRC32／MD5 的实际构建、执行、诊断、悬停、补全、定义与间接头变化撤回；共 10 次底层扩展命令通过。完整重链接检查包含 8 个实际顶层命令：冷构建、两份 DLL 执行、两份环境查询、混编、运行和 IDE 扩展检查。

## 检查工具

[check_compiler_relink.py](../tools/check_compiler_relink.py) 从指定包清单复制源码，保留修改前后字节、差异、原始输入、实际进程身份、退出码及日志。所有编译／DLL／IDE 检查都在新建的私有 Windows 桌面中执行，不切换用户桌面。输出目录必须不存在；失败输出保留。

```powershell
python tools/check_compiler_relink.py `
  --package dist/portable-20261011-c-extensions-v1/xlang-ide `
  --gcc E:/software/w64devkit/bin/gcc.exe `
  --fixture build/cb-6bcdf568/release/c_extension_test.exe `
  --output build/compiler-relink-new
```

GCC 是重建时依赖，IDE 运行不因此新增开发工具依赖。夹具必须来自对应的实际 IDE 源码构建，不能将任意同名程序当作已核验夹具。工具的当前 1,532 输入／2,085 扩展合同针对本候选，其他来源版本须另行明确核对。

首轮构建、两份 DLL 执行和两份查询都通过，但检查器随后把环境 JSON 交给 16 MiB 的包元数据读取器而失败。C 环境的既定上限是 64 MiB；修正只应用于这个响应解析路径，包元数据原上限保持。原脚本字节和首轮失败回执保留。续验先复核原输入、输出、日志和实际 DLL 执行结果，再完成后续检查，没有重复冷构建或把首轮整体失败计作成功。

## 证据与范围

- [实际冷构建及首轮整体失败](../build/compiler-relink-v1-20261011/evidence.json)
- [修正后的完整重链接验收](../build/compiler-relink-v2-20261011/evidence.json)
- [修改前后 TCC 与输入记录](../build/compiler-relink-v1-20261011/source-inputs.json)
- [运行版 DLL 实际新函数](../build/compiler-relink-v1-20261011/xl.dll-probe.json)
- [编译版 DLL 实际新函数](../build/compiler-relink-v1-20261011/xlc.dll-probe.json)
- [最终核验](../build/compiler-relink-final-v1.json)

原便携包、默认编译器、固定 SDK 和已有发行材料不变；重建输出仅用于技术验证。本轮没有重新执行 IDE 应用完整回归或重新组装便携包，原有 774 份严格构建输入与 18 项当前候选绑定另行复核一致。

干净 Windows、实体输入法与混合 DPI 的设备验收仍缺少实际证据。本机当前没有可调用的 Windows Sandbox／Hyper-V 工具；在线功能状态查询要求当前执行上下文没有的提升权限，因此不推定这些系统功能已禁用。已有开发机便携测试不能代替干净系统安装。长期目标继续。
