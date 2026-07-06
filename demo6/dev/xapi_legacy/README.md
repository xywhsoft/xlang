# XAPI legacy archive

本目录保存 DEMO6 曾经实现过的 XAPI 源码、测试、fixture 和规则文档。

当前主线已经改为 source import：

- `import` 读取 `.xl` 源码或 package manifest 中的 `xlSources`。
- package manifest 使用 `xlang.package.json`。
- 同名 `.c` companion 由 `.xl` 入口或 import 自动纳入。
- 主线不再识别 `package.xapi.json`。
- 主线不再提供 `xl xapi` 或 `xlcEmitXapi`。

本目录只作为迁移参考，不作为当前设计或实现来源，不参与 `build.bat`、`test.bat` 或发布产物。
