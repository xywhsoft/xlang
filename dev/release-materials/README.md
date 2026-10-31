# 发布开发资料

此目录保存从根 `release/` 迁出的开发材料，运行程序继续位于根 `release/`。

- `sources/`：原有 xlang、XGE、TCC 源码快照及独立重建说明。
- `provenance/`：原有来源、构建、兼容补丁、交付及验证报告。`current-delivery.json` 是当前交付索引。

2026-10-31 迁入 2,427 个文件、108,473,552 字节，按原字节移动并逐文件核对 SHA-256；历史报告中的绝对路径记录当时环境，不批量改写。当前交付索引及运行目录中的安装／材料索引更新为新位置。迁移原始清单保存在 `ide/build/Release/materials-move-20261031/migration.json`。

源码快照和已有生成报告沿用原来的本地资产管理方式，不重复提交完整源码副本；原先跟踪的三份交付报告继续在 Git 中跟踪。迁出目录未被删减。不要用清理构建缓存的方式删除本目录。

构建脚本的固定前端备用目录改为 `dev/release-materials/sources/xlang`，显式 `-FrontendRoot` 仍有效。默认便携组装将源码和来源记录放在候选旁的 `development-materials/`，运行目录和运行 ZIP 中不再放 `sources/` 或 `provenance/`。独立开发资料通过清单摘要与同批运行程序绑定，校验工具兼容旧候选布局。
