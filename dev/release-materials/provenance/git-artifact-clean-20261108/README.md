# Git 产物历史清理

2026-11-08 已在本地重写 2,837 个提交、25 个引用（包括分支、远端跟踪、标签与 Codex 树检查点）。剔除 release/ 及明确列出的历史旧位置，并按用户确认移除 103.45 MB 的开发快照 ZIP。保留全部提交及合并拓扑；其余文件的字节与权限逐提交核对一致。

Git 对象占用从约 6.01 GiB 降至 728.94 MiB，剩余最大 blob 为 54,532,578 字节。旧对象及 reflog 已回收；git fsck 完整检查通过。没有删除磁盘运行文件：14,471 个 release 文件、指定 ZIP、6 个工作区的源码修改与非产物暂存项均核对保留。

.gitignore 移除发布目录的反向放行规则；AGENTS.md 明确禁止 force-add 运行包。旧分支的本地关联工作区另有 Git info/exclude 防护，不修改它们的源码文件。交付检查改为验证本机文件与清单摘要一致，并拒绝跟踪 release/。全新克隆需要先取得配套运行包。在线 ver2 发布内容未变。

恢复资料位于 F:/xlang-validation/release-history-clean-20261108/。before.bundle 为经过完整性验证的独立旧历史备份，约 1.10 GiB，不在当前仓库或推送内容中；worktrees/ 保存旧索引和未提交源码快照。commit-map.tsv、ref-map.json 对应新旧 ID，历史报告中的原始字节与旧 ID 不作改写。

## 远端同步

本轮没有覆盖 Gitee 远端。历史 ID 已变，普通 push 会因非快进被拒绝。需要由发布者确认并更新 master 与既有 0.6.0-Preview 标签；其他克隆应先保护本地修改，再重新克隆或迁移到新历史，避免合并旧历史重新带回产物。

以下命令只允许覆盖本次检查时看到的远端值；远端已被其他人更新时会拒绝操作，不要改成无条件 --force：

```powershell
git push --atomic --force-with-lease=refs/heads/master:596cab8f6b015c2d193e72c8dd39e3587c0178cc --force-with-lease=refs/tags/0.6.0-Preview:0e30a18a9fd756f19824100b1f187840fbd4a0c1 origin refs/heads/master:refs/heads/master refs/tags/0.6.0-Preview:refs/tags/0.6.0-Preview
```

若远端不支持原子推送或受保护分支禁止强推，应先按远端提示处理；不要使用 push --mirror，以免把本地工作分支和检查点一起发布。远端旧对象的物理空间回收由托管平台控制，本次占用数据是本地实测。
