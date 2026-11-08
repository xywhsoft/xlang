# Git 与运行产物

`release/` 整棵目录保留在本机，通过发行包和在线更新通道交付，不进入 Git，也不要使用 `git add -f` 重新加入。这里的 SDK、扩展库、示例、资源和头文件同样适用。新克隆的仓库需要先获取匹配的运行包，再使用依赖它的构建或验收工具。

源码与开发材料继续放在既有开发目录；运行产物的版本、来源和 SHA-256 记录在 `dev/release-materials/provenance/`。`tools/check_release_committed.py` 保留原入口名称，现在检查本机运行文件与交付清单是否一致，并拒绝重新跟踪 `release/`。

2026-11-08 清理了发布目录及其旧位置的 Git 历史，并额外移除了经确认的 `dev/release-materials/sources/xlang-development-20261105-88bd73dc-r1.zip`。本机文件未删除，历史提交和标签的 ID 已改变。历史验收报告中的旧 ID 和记录内容保持原样，可用本次提交映射查找新 ID。

详细范围、验证和恢复位置见 [清理记录](../dev/release-materials/provenance/git-artifact-clean-20261108/README.md)。现有克隆同步时应先保存自己的修改，重新克隆或按新历史迁移；不要把旧历史再次合并回主线。
