# Git 仓库体积与开发材料

源码仓库保存源码、许可证、构建所需 SDK、必要文档、小型验收报告及材料摘要。运行发行包继续位于被 Git 忽略的 `release/`。大型开发快照、验收归档、截图、原始大日志、第三方下载工作副本与模型资源通过独立材料包提供。

历史报告的原始字节、SHA-256 和记录路径保持不变。被迁出的文件保留在本机原位置，只取消 Git 跟踪并清除历史对象；新克隆通过恢复工具取回。不能仅在当前提交删除文件，否则历史体积仍会增长。

## 恢复开发材料

```sh
python tools/dev_materials.py list
python tools/dev_materials.py restore dev/release-materials/provenance/release-0.6.2-20261109/evidence.zip
python tools/dev_materials.py --all-current restore
```

清单位于 `dev/release-materials/materials-index.json`，包含不可变 HTTPS 材料包地址、大小、SHA-256、原路径及当前文件摘要。完整历史路径和版本目录保存在材料包中。工具先校验包和目录，再校验每个恢复对象；默认拒绝覆盖内容不同的用户文件。`--force` 才允许显式替换。历史版本可用 `--blob <Git blob ID>` 选择，离线材料包用 `--bundle <文件>` 指定。恢复缓存位于被忽略的 `dev/local-state/material-cache/`。

历史回执不因材料迁移而改写。新增恢复索引解释旧路径与外部材料的对应关系；需要整套历史开发证据时先恢复材料。普通编译与运行不需要下载整套验收归档。

## 体积门禁

```sh
python tools/install_git_budget_hook.py
python tools/git_repository_budget.py --output build/git-budget.json
python -m unittest discover -s test -p test_git_repository_maintenance.py -v
```

门禁按待推送主线/标签的可达历史生成独立 pack 并计量，不使用工作目录大小代替仓库大小。250 MiB 提醒、350 MiB 阻止推送；普通新增文件超过 1 MiB 提醒、5 MiB 默认拒绝。经过审查的现有大源码、SDK、字体和文档对象按指纹固定在策略文件中；更新它们时重新审查对应对象，不能把整个目录无限放行。

门禁检查历史路径，直接 force-add 归档或把它从旧分支合并回来也会拒绝。恢复工具下载的材料仍应留在忽略目录中。安装脚本保留并串联现有 pre-push（包括 Git LFS），不替换其他钩子；自定义共享 hooksPath 需要手动集成。CI 中提供相同检查入口。

历史重写前必须备份全部分支、标签、检查点、各工作区索引及未提交修改。只移除获准的产物路径，逐提交验证其余内容、权限和合并关系，最后按远端实际 ID 使用 `--force-with-lease` 同步。不要使用 `--mirror` 发布内部工作分支或检查点。推送完成后再执行服务端 GC。
