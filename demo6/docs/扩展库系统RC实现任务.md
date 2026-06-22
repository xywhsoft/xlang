# DEMO6 扩展库系统 RC 实现任务

本文档记录本轮长期任务的实现边界与验收标准，避免后续开发时混入未定稿能力。

## 目标

本轮完成三块能力：

- 模块化 / import / package。
- XAPI / 接口元数据。
- 扩展库系统。

目标成熟度：当前已实现能力必须完整、稳定、可用，达到发布候选版本的工程基线。

## 必须实现

- package manifest 读取：支持 `xlang.package.json`，读取 `name`、`version`、`abiVersion`、`xapi`、`artifact.cSources`、`links`、`loadlibs`、`dependencies`、`native`。
- package instance identity：形成 `packageId = name@version#abiN`，并写入 resolver / XAPI / import 内部模型。
- XAPI 元数据补齐：支持读取和生成 `package`、`moduleId`、`links`、`loadlibs`、`cSources`、导出签名身份字段。
- import 命名空间隔离：`import counter` 后只通过 `counter.xxx` 访问，不把导出符号直接注入全局。
- import 依赖私有：包内部依赖不自动暴露给上层模块。
- native singleton 冲突检测：同一构建图中冲突的 singleton native artifact 必须报错。
- 自动链接消费：`xl run/build main.xl` 能消费导入包声明的 `cSources`、`links`、`loadlibs`。
- C/XLang 混编保持零配置方向：用户显式传入 `.c` 文件仍自动注入 XRT 与生成头。
- 示例与测试：覆盖函数导入、变量导入、缺失接口、破损 XAPI、不支持导出、命名空间隔离、包内 C source 自动编译、native singleton 冲突。

## 暂不实现

- 远程包下载。
- 完整包仓库。
- 完整 semver range 求解器。
- `import C1 = C@1.0.0` 语法。
- re-export 语法。
- 二进制 `.xapi` 格式。
- `xlang.lock` 的完整复现逻辑。

## 验收

- `demo6\build.bat` 可以构建。
- import/XAPI/package 相关正向与负向测试稳定通过。
- `demo6\rc0_guard.bat` 通过。
- 不新增 demo5 兼容回退口径。
- 不绕开 XRT 已有文件、内存、JSON、缓冲区能力。
