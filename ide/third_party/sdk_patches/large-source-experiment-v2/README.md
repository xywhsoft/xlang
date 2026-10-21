# 大源文件编辑：可见范围缓存实验

这是 2026-10-10 的第二版私有 SDK 实验，未安装到发布 SDK。它保留第一版的 Unicode 类别、Emoji 根节点、绘制和行布局修改，只在 src/xui_code_edit.c 增加本轮优化。公开头未修改，XRT 仍由 xge.dll 单独提供。

水平滚动到超长行末尾时，旧实现分别从行首扫描可见起点和终点。本版通过一次字素遍历找到两者，并保留旧的严格边界规则。每个编辑器只缓存一个当前读取缓冲区的查询结果，额外状态为固定数量的字段，不增加文本或字形副本。

缓存绑定文档身份、文档版本、读取范围、起止视觉列和 Tab 设置。读取缓冲区即将重写时先失效，包括内存分配或复制失败；清理行布局时也失效。外部文本和不匹配的读取范围始终走计算路径。字体或视口变化通过重新计算起止视觉列影响查询，结果没有改变字素分段、整形、语法或绘制规则。

新旧实际函数片段的等价检查完成 461,674 项断言，覆盖 Unicode、Tab、部分范围、文档版本、缓冲区复用、视口变化和失败路径；相同查询的缓存命中没有再次调用字素遍历。夹具的严格构建使用 -Werror；完整 SDK 构建沿用原来的 -O2 -Wall -Wextra 配置，没有使用 -Werror。真实窗口测量及限制见 [本轮记录](../../../docs/large-source-viewport-cache.md)。

## 重新准备源码

在 D:\GIT\x-lang\ide 执行，目标必须是尚不存在的新目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_viewport_cache_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\viewport-cache-sdk-review'
~~~

工具沿用第一版的固定 SDK、原始材料和补丁字节检查，生成 237 个材料与 source-preparation-report.json。它只准备独立源码，不构建、不安装。patch-manifest.json 绑定 5 个完整补丁材料和 diff，也记录本轮实际 SDK 构建和等价检查。

进入发布前仍需解决长行其余热点，并完成绘制裁剪、完整 IDE 回归、成套 SDK 替换、应用严格构建及便携包验收。有限原生测量不代表实体 IME、混合 DPI、干净 Windows 或正式首版已验收。
