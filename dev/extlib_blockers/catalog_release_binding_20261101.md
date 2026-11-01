# 扩展库命令目录候选：待统一主线发布基线

## 当前证据

2026-11-01，普通扩展库BSON阶段已从主目录生成完整命令/来源候选：
207个公共包、17598个条目，保留6156个非扩展条目，公开项新增/删除均为0。
候选包含BSON本次声明和README的实际来源字节，不修改IDE读取器。

发布前的严格校验未通过：

```powershell
python -B -X utf8 ide/tools/check_catalog_coverage.py --root D:/GIT/x-lang --catalog build/test/bson_current_20261101/catalog/commands.json
```

实际退出1，原因是内部的`git diff --quiet`确认当前`lib/`不再等于
固定发布基线`ed78373dded9bde21db313799c1db8d5111631a6`。
并行线程随后通过`44ac8a20b`将XRT从
`3fde6da5732d54feeea4ed1b0db5b42ad0f1867c`同步至
`93347eb5ae6beba8cf7ee3558708cc4f628fdd07`，包括网络内部初始化与
静态ownership表修正。这些不是本任务的改动，也不是BSON导致的API缺失。
之后又替换`release/xl.dll`和`xlc.dll`，现行严格校验首先报
`Catalog compiler changed: xl.dll`，交付原始字节审计也报DLL已变化。
本任务分别保留新旧发布物的实际测试哈希，不替并行工作改写旧验收报告。

当前候选只完成提取，并未成为已验收发布资料。不能仅把旧条目和来源
复制过去、跳过覆盖检查、修改旧发布报告或把并行XRT改动加入本任务提交。
本次未覆盖`ide/data/`、`release/data/`或交付索引中的既有资料摘要。
它们在BSON更新后仍是旧快照，故本轮源码提交不代表命令目录交付完成。

CBOR阶段再次生成包含BSON/CBOR最新来源的候选（同为207包/17598条目），
在更新后的DLL上提取且新鲜度检查通过，但严格发布绑定仍未通过。
候选位置为`build/test/cbor_current_20261101/catalog-updated-runtime/`；
输入/工具原文均已在仓库，候选属于可重新生成的中间产物，不是唯一源码。

MessagePack与CLI M7复验后又生成包含最新声明、manifest和README的
`build/test/msgpack_current_20261101/catalog-final-2/`，仍为207包/17598条。
其新鲜度检查通过，严格绑定仍失败，因此未安装。此时交付字节审计
已恢复通过，说明并行线程已提交相应运行文件，但不能据此绕过基础
目录的编译器/源码绑定检查；期间另有并行主线源码改动，继续独立维护。

## 收口要求

2026-11-01 Redis字节预算修复阶段尝试重生成资料时，前端构建先在
`src/parser/xl6_parser_api.c:73` 失败：`XL6PG_KW_STRUCT`未声明。
此时解析器源码正在另一线程更新；不复制旧生成头、不加回退宏，也不
在扩展包任务中修主线。现场为`build/test/redis_pending_20261101/catalog-generation.json`，
已安装目录不动。Redis公开API未变，新增README/范例需要的来源更新
仍待该前端及原有基础发布绑定统一后重新生成，不能复用旧候选假装新鲜。

随后SQLite WAL阶段核实该token已由并行线程补齐，重新生成成功；
`build/test/sqlite_wal_20261101/catalog/`包含最新Redis、SQLite等资料，
新鲜度通过，严格绑定仍报`Catalog compiler changed: xl.dll`，因此
未安装。提交字节审计仍通过；这既不证明旧token故障仍存在，也不证明
发布绑定已经关闭。本扩展任务没有修改解析器或替主线改写基线身份。

1. 主线维护者完成稳定XRT同步、相匹配的编译器发布物和验收记录。
2. 如标准库/主线绑定改变，先按当前正式验收报告重生成基础命令目录。
3. 再刷新扩展资料候选，运行真实前端声明检查及严格覆盖/新鲜度检查。
4. 成对发布到`ide/data`和`release/data`，更新
   `dev/release-materials/provenance/current-delivery.json`后立即提交。
5. 对已提交运行文件执行`tools/check_release_committed.py`。若并行IDE
   产物尚未交付，保留其自身失败项，不替其他线程改写产品哈希。

本任务继续推进包本体、测试和双语范例；此发布阻断不扩大为所有包
功能不可用，也不以它为理由缩小100包的长期目标。
