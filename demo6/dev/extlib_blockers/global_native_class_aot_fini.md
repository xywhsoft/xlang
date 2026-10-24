# 顶层 native-backed class 在独立 EXE 退出时失败

2026-10-24 使用当前源码构建的 `public_source.exe` 验证，不使用旧发布版。

从 `demo6` 目录设置 `$env:XLANG_PACKAGE_PATH = (Resolve-Path release/extlibs).Path`，
分别对以下三个文件执行 `run` 和 `exe`，然后运行生成的 EXE：

| 文件 | 内存运行 | 独立 EXE |
| --- | --- | --- |
| `global_native_class_aot_fini.xl`：顶层保存 `blake3.hasher()` | 打印 `ready`，退出 0 | 打印 `ready`，随后抛 `RuntimeError: operation is not valid in the current state`，退出 1 |
| `native_class_import_aot_fini_control.xl`：仅导入 | 退出 0 | 退出 0 |
| `native_class_local_aot_fini_control.xl`：函数局部 hasher | 退出 0 | 退出 0 |

公开 `blake3_basic.xl` 与 `blake3.xl` 同样在独立 EXE 打印完正确结果后失败，
而源码驱动的内存运行通过。BLAKE2 公开范例也出现相同的退出错误，
但尚未证明其最小根因相同。

生成的 `module.c` 在 `xlir_module_fini_body` 中释放顶层的 `xvalue*`
对象槽；报错发生在用户代码执行完毕后的模块退役阶段。需要主线检查
顶层 native-backed class 的所有权、析构调用与模块退役顺序，特别是
清理时是否仍允许调用所属代码及原生析构。修复后应分别复验三份最小
文件，以及 BLAKE2/BLAKE3 公开范例的 JIT、独立 EXE；不能仅把范例
移进 `main` 来掩盖全局对象缺陷。本普通扩展包任务不修改主线。
