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

ZIP解压修复阶段再次生成207包/17598项候选，生成期间输入不变；随后
新鲜度检查报告并行`src/ir/xl6_ir.h`又变化，严格绑定仍报`xl.dll`。
现场在`build/test/zip_extract_20261101/catalog*`。后续README补充最终
结果也必须重新捕获；不把已生成候选冒充当前新鲜目录，不安装或改基线。
最终文档补齐后已重生成`catalog-final`，新鲜度通过，严格绑定仍失败。
它包含本次ZIP最终README/manifest字节，仍未成为已安装交付物。
随后等价路径补测与回归修正的最新候选在
`build/test/zip_paths_20261101/catalog/`，207包/17598项，新鲜度通过、
严格xl.dll绑定失败，仍未安装；对应资料源与完整报告已纳入源码提交。
11月2日INI显式帧阶段最终候选为`build/test/ini_depth_20261101/catalog-final/`，
包含最新README和coverage资料，生成时新鲜度通过、严格绑定仍报
`Catalog compiler changed: xl.dll`，未安装。完整结果在
`docs/reports/extlib-ini-depth-20261102.json`，未替主线改基线或复制旧资料。

11月2日HCL标量修复阶段生成`build/test/hcl_scalar_20261102/catalog/`，
207公共包、17598条目，新增/删除公开项均0，包含最新HCL声明/README/
coverage。新鲜度通过，严格绑定仍报`Catalog compiler changed: xl.dll`，
未安装。609份运行文件/资料的原始Git字节审计通过，不将它替代发布绑定。
完整结果见`docs/reports/extlib-hcl-scalars-20261102.json`。

同日TAR文本保真阶段最新候选为`build/test/tar_text_20261102/catalog/`，
207包/17598条目，公开项增删均0；README/coverage已捕获，新鲜度过，
严格绑定仍报xl.dll，未安装。官网仅隔离预览，609份运行原始字节审计过。
包内及保留的M1/RFS实测见`docs/reports/extlib-tar-text-20261102.json`。

stb_image_write HDR阶段生成`build/test/stbiw_hdr_20261102/catalog/`，
207包/17598项，新增/删除公开项0；生成时输入一致，随后新鲜度检查因
并行`src/ir/xl6_ir.h`变化失败，严格绑定仍报`Catalog compiler changed: xl.dll`。
候选未安装，不循环重生成碰运气；源码/README/coverage可据已提交来源重建。
609份交付原字节审计通过，不能替代这两项门禁，完整结果见
`docs/reports/extlib-stbiw-hdr-20261102.json`。

Tabulate输出/返回阶段候选`build/test/tabulate_render_20261102/catalog/`
生成并通过候选自身新鲜度（`--data <candidate> --check`），207包/17598项，
公开项增删0。已安装资料的新鲜度仍失败，不混作候选漂移；严格绑定
仍报`Catalog compiler changed: xl.dll`，因此未安装、未改基础绑定。
官网源已更新并作隔离预览，609份交付原字节审计过；本轮包级M2和
候选/安装检查的实际区分见`docs/reports/extlib-tabulate-render-20261102.json`。

Netrc引号字段阶段候选`build/test/netrc_quoted_20261102/catalog/`生成，
207包/17598项、新增/删除公开项0，新鲜度通过。严格绑定仍报xl.dll，
候选未安装、未重写基础来源；官网源已更新并有隔离预览，609运行原
字节审计通过。本包M1仍保留，结果见`docs/reports/extlib-netrc-quoted-20261102.json`。

Semver元数据存储阶段候选`build/test/semver_metadata_20261102/catalog/`
生成并通过候选自身新鲜度：207包/17598项、公开项增删0，145文档/
593来源，包含最新README和coverage。严格绑定仍报
`Catalog compiler changed: xl.dll`，因此未安装，也未改写基础编译器身份。
官网仅保存包内源页并作隔离副本，609份运行文件原始Git字节审计通过；
它不替代绑定验收。完整包级结果见
`docs/reports/extlib-semver-metadata-20261102.json`。

Redis长度头修复阶段最终候选
`build/test/redis_resp_20261102/catalog-final/`包含最新README/coverage，
207包/17598项、公开项增删0，新鲜度通过；严格绑定仍报
`Catalog compiler changed: xl.dll`，未安装、未改写基础编译器身份。
初次候选在最终验收文字补齐前生成，已明确由最终候选替代，不冒充
来源仍新鲜。官网源有隔离副本，609份运行原始Git字节审计通过；
包级结果与原R风险见`docs/reports/extlib-redis-lengths-20261102.json`。

CLI旗标查找阶段候选`build/test/cli_flags_20261102/catalog/`生成，
207包/17598项、公开项增删0；最终README和三个范例coverage来源已捕获，
候选新鲜度通过。严格绑定仍报`Catalog compiler changed: xl.dll`，未安装、
未改写基础身份；官网仅隔离副本，609运行原始Git字节审计通过。
包级八条M1运行失败继续登记，未用JSON例或候选生成替代完整验收。
具体结果见`docs/reports/extlib-cli-flags-20261102.json`。

JSON Patch独立语义阶段候选
`build/test/json_patch_semantics_20261102/catalog/`生成，207包/17598项，
公开项增删0，最终README/coverage来源已捕获且新鲜度通过。严格绑定
仍报`Catalog compiler changed: xl.dll`，未安装、未改写基础身份。
官网源有隔离副本，609份运行原始Git字节审计通过；包级M1正例仍红，
不能以新8133项或候选代替发布。详见
`docs/reports/extlib-json-patch-semantics-20261102.json`。

Dotenv普通export键阶段候选
`build/test/dotenv_grammar_20261102/catalog/`已生成，207包/17598项，
公开项增删0，最终README和新增语法/范例coverage已捕获，新鲜度通过。
严格绑定仍报`Catalog compiler changed: xl.dll`，因此未安装，也未改写
基础身份或交付索引。官网源有隔离副本，609运行原始Git字节审计通过；
本包M2顶层正例仍红，详见
`docs/reports/extlib-dotenv-grammar-20261102.json`。

FIX校验不物化阶段候选`build/test/fix_validate_20261102/catalog/`
生成并通过自身新鲜度：207包/17598项，公开项增删0，145文档/593来源，
包含最新README与语法/范例coverage。严格绑定仍报
`Catalog compiler changed: xl.dll`，未安装、未改写基础身份或交付索引。
官网源有隔离副本，609运行原始Git字节审计过；本包原M1构帧正例
继续失败，未以九普通AOT或合法谓词不分配替代发布签收。详见
`docs/reports/extlib-fix-validate-20261102.json`。

CSV数据BOM阶段候选`build/test/csv_bom_values_20261102/catalog/`
生成并通过自身新鲜度：207包/17598项，公开项增删0，145文档/593来源，
最新README/coverage已捕获。严格绑定仍报xl.dll变化，未安装、未改写
基础身份或交付索引。官网源有隔离副本，609运行原始Git字节审计通过。
本包M2/M3/M5及Windows RFS仍开放，不以数据BOM修复替代发布签收；
完整记录见`docs/reports/extlib-csv-bom-values-20261102.json`。

1. 主线维护者完成稳定XRT同步、相匹配的编译器发布物和验收记录。
2. 如标准库/主线绑定改变，先按当前正式验收报告重生成基础命令目录。
3. 再刷新扩展资料候选，运行真实前端声明检查及严格覆盖/新鲜度检查。
4. 成对发布到`ide/data`和`release/data`，更新
   `dev/release-materials/provenance/current-delivery.json`后立即提交。
5. 对已提交运行文件执行`tools/check_release_committed.py`。若并行IDE
   产物尚未交付，保留其自身失败项，不替其他线程改写产品哈希。

本任务继续推进包本体、测试和双语范例；此发布阻断不扩大为所有包
功能不可用，也不以它为理由缩小100包的长期目标。
