# 编译器深文件路径

IDE 使用固定的独立 xlang 编译器。2026-10-10 的修复只应用到该编译器副本；当前根仓库 `demo6` 不作为这次构建的源码输入。

修复覆盖两处已复现的失败：普通 Win32 文件路径导致深目录请求清单不可读，以及 CLI 项目目录/输入路径的 512 字节缓冲导致 IDE 临时请求清单加载失败。文件系统边界先用 `GetFullPathNameW` 解析相对路径和点段，再对长路径添加原生扩展前缀；短路径、公开逻辑路径、进程工作目录、相对符号链接目标和句柄相对名称保持各自语义。参见 [Windows 路径说明](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file) 和 [GetFullPathNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfullpathnamew)。

XRT 与 xruntime 的文件读写、元数据、移动/原子替换，以及目录枚举、创建和移除使用此转换。TCC 的磁盘 open/fopen/unlink 同样处理长文件路径，fopen 模式字符串只做 UTF-16 转换。编译输入报告的存在性探测改用同一 XRT 元数据接口，区分缺失、非普通文件和访问失败。文件摘要、身份检查、失败搜索候选以及产物重叠拒绝继续执行。XRT/xruntime 元数据明确记录这是一项 IDE 私有适配，没有宣称重新生成上游快照；公共声明和 API 不变。

CLI 项目清单与解析后的输入缓冲调整到 4096 字节，与现有构建输入报告的限制一致。清单路径超过 4095 个 UTF-8 字节时，在读取之前拒绝。开启和关闭输入报告均用长度固定的消息说明限制，保留原产物；不会把超长路径再次放入格式化错误消息。此范围不等于支持任意长度的所有工具链路径。

`tools/check_compiler_deep_paths.py` 使用实际 `CreateProcessW` 和从不切换的私有桌面。项目清单、C/xlang 源码和带中文/emoji 的头文件均位于真实深目录；头文件路径包含点段。原编译器在相同目录下失败，原产物不变。新编译器需构建、输出 C/头文件、通过语义检查，并在输入报告中绑定原始文件与产物摘要。已有可核验短名的深工程还需由实际 IDE 构建并运行；C 的相对读取必须命中原工程文件。无短名深目录的文件访问从普通工作目录验证，IDE 工作目录启动仍应明确拒绝，并保留旧输出。

可复现适配保存在 [补丁](../third_party/compiler_patches/native-file-paths-v6/native-file-paths.patch) 和 [字节替换清单](../third_party/compiler_patches/native-file-paths-v6/changes.json)。[准备工具](../tools/prepare_native_path_compiler.py) 校验固定的 1512 份种子源码和七份改动的前后摘要，创建全新副本后应用适配，逐项核对副本与种子。它不修改种子，不接受已有输出目录，也不把准备成功计为构建成功。

```powershell
python -X utf8 tools/prepare_native_path_compiler.py `
  --seed-root build/class-scalar-abi-v2-compiler-0f36bffbbd314519ba0fae026f19ac8e `
  --source-manifest build/class-scalar-abi-v2-compiler-0f36bffbbd314519ba0fae026f19ac8e/source-inputs-class-scalar-abi-v2.json `
  --output build/my-native-path-compiler
```

进入新副本运行 `build.bat`。随后运行编译器兼容性检查与深路径验收；完整 IDE 回归及独立便携验收是发布开发候选的后续条件。种子清单 SHA256 为 `2807cd355c7d0bef9c7a4322088446d9d9f70e3fa9b533208c1a97c9cd81ebd6`。

无可用短名的深工作目录、完整禁用短名的卷、ReFS/UNC、动态库深路径加载、所有外部工具和正式发行的设备/环境覆盖继续属于未完成范围。详见 [任务和工具链](tasks-and-toolchain.md) 与 [深程序路径启动](startup-paths.md)。
