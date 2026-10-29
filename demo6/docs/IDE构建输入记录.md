# IDE 构建输入记录

当前新增接口处于独立工具链验收阶段。它为一次实际构建记录磁盘输入，供 IDE 在发布和运行产物前复核；不会扫描源码字符串来推测依赖。

```text
xl build project.xlp -o private/program.exe --input-report private/inputs.json
xl build main.c --inc-path include -o private/program.exe --input-report private/inputs.json
```

`--input-report` 只用于 `build`，只接受一个非空文件路径。支持 `.xl`、`.c`、`.xlp`、C 混编、应用或共享库、资源及实际链接输入。当前要求静态 TCC/VFS 和磁盘源码；与源码 overlay、`--loadlib`、`--emit-header`、`--save-temps` 的组合明确拒绝，stdin 链接输入也拒绝。动态 TCC、运行程序的后续文件访问及 DLL 装载依赖不属于本接口。

## 同次构建的输入

编译上下文保留同次 xlang 解析、导入和包描述的源码报告。TCC 只读 `open`/`fopen` 在动态内存资源之后、磁盘及内嵌资源之前接入观察 transport：真实磁盘输入首次读取时用同一文件句柄获取原始字节、摘要、大小、身份及时间；随后重复读取使用同一份不可变字节。内嵌资源和编译器生成的内存源码继续沿用既有优先级。

Windows 资源配置及其实际选中的图标/应用清单、项目清单使用相同磁盘读取 transport。C 宏、传递 include、链接对象或库的输入来自 TCC 的真实读操作。失败的原生候选路径和 xlang 的普通文件候选谓词独立记录；自动匹配的同名 C 文件及外部运行库源码选择也受观察。未读取且未参与候选判断的文件不会被当作构建输入。

完成代码生成、链接及产物写入后，编译器复核所有已观察输入、候选和 `XLANG_PACKAGE_PATH`，将新产物的路径、摘要及身份绑定到该上下文。写记录时再次校验，已变化的文件或产物不能取得成功记录。输出和记录路径不能覆盖已观察输入、硬链接别名或失败候选路径；记录也不能覆盖产物。

失败可能保留旧输出或旧记录文件。调用方必须使用本次任务的独立目录，要求编译进程成功退出，并确认本次新记录及产物；禁止通过旧文件的存在判断构建成功。

## 协议

JSON 顶层使用 `schema: 1`、`scope: "xlang-native-build-disk-inputs"`、`complete: true`。`artifact` 和 `files` 的记录包含：

- 绝对 `path`、原始字节 `size`、64 个十六进制字符的 `sha256`；
- `metadata.available`、`attributes`；
- 以字符串表示的 64 位 `device`、`identity`，以及 `modifiedUs`、`changedUs`。

`nativeMissing` 是只读打开时缺失的磁盘候选路径；不可读或非普通文件不会被伪装成缺失。`xlangProbes` 保留 xlang 普通文件谓词的路径与布尔值。`packagePathObserved` 和 `XLANG_PACKAGE_PATH` 保留实际观察的环境状态，区分未设置与空字符串。

调用方还必须绑定完整命令参数、配置、编译器及兄弟 DLL 指纹，验证记录格式、范围和文件身份/摘要；在发布及运行前重新验证，保留记录和原始构建日志。记录不替代调用方的任务版本、脏文档通知或工具链守护。

当前原生磁盘快照最多 1,024 文件、单文件 64 MiB、合计 256 MiB，缺失原生候选最多 4,096。xlang 来源报告继续使用既有 512 文件、单文件 8 MiB、合计 64 MiB及候选限制。超限、读取错误或无法取得稳定身份时失败，不降级为“完整”记录。

## 验证及 Windows 构建

`tools/check_build_inputs.py --compiler-root <独立编译器源码根> --work-root <验收目录>` 编译快照专项测试，执行真实 C/xlang/混编/资源/链接构建，并通过 compiler API 在构建后改变导入、头文件或产物，验证成功记录被拒绝。每次运行保留自己的目录、命令、输出及 `evidence.json`；本接口尚未完成 IDE 任务服务接入和发行验收。

Windows DLL 编译的长源文件列表改为 `tools/runtime_dll.rsp.in` / `compiler_dll.rsp.in`。`build_dll.bat` 调用 `build_release_dll.ps1` 展开工具链变量，在构建目录保存实际参数文件，再调用 GCC，避免 cmd.exe 的 8 KiB 命令限制。SDK 及运行时公开 API 的边界不变。
