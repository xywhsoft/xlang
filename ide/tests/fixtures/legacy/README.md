# 历史旧清单夹具

`workspace/` 中六份文件逐字节来自 x-lang 仓库提交 `f09a27a759adf7eb4ab9e2f7fd292ed57e3418e6` 下的 `ide/test/fixtures/workspace/`，使用 `git show` 读取，未恢复或修改原历史目录。

夹具包含 [Workspace1.xlw](workspace/Workspace1.xlw)、[Project1.xlp](workspace/Project1/Project1.xlp)、原 xlang/C 源、XSON 数据和工作空间说明文件。清单中的 `res/app.ico` 在原样本中不存在，导入应保留缺失引用并报告，不生成伪图标。

此夹具验证格式和数据保留。真实 DEMO6 混编验证由 `legacy_import_test.c` 在独立 GUID 测试目录生成当前语言语法的入口和 C 源，不能据此宣称旧源码自动适配新语义。

格式和早期行为另以只读的 `D:\GIT\xlang_ide` C# 实现核对，详见 [旧格式导入协议](../../../docs/legacy-import.md)。
