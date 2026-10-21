# xlang 编译器私有深路径适配

这七份源码/快照元数据改动来自 IDE 验收使用的固定编译器副本：Win32 原生长文件路径、TCC 磁盘 I/O、构建输入探测和 CLI 项目路径缓冲。公共 API 不变，不修改根仓库 `demo6`。

`native-file-paths.patch` 供代码审阅；`changes.json` 记录精确字节替换和前后 SHA256。[准备工具](../../../tools/prepare_native_path_compiler.py) 只接受已固定的 1512 文件种子清单，创建新目录并核对全部源码。必须随后执行真实构建和验收。

保留种子中的 XRT/xruntime 和 TCC 授权声明。快照元数据明确标记 IDE 私有适配，不宣称这些字节是重新生成的上游快照。范围、复现命令和限制见 [编译器深路径说明](../../../docs/compiler-native-paths.md)。

本版另在清单读取前检查 4096 字节上限，开启和关闭输入报告均给出有界错误。v5 补丁和原验收候选继续保留。
