# xlang 0.6.2 交付与验收

版本 0.6.2 / 0x00000602，ABI 1，保持 -O2。两平台版本、SDK、JIT/AOT、嵌套编译和交付检查通过；IDE 关于、启动、输入、查找、设置、首页和模板通过。

清理删除 6 个临时/锁文件，归档 7 个范例缓存文件，共从运行目录移出约 17.62 MiB。用户设置和诊断保留。

`receipt.json`、`cleanup-summary.json` 与 `evidence.zip` 记录精确输入和结果；源码增量在对应 sources 目录。签名 ver3 上线见相邻 `ide-update-v3-publish-20261109`。未执行全仓回归，历史限制保留在 receipt 中。
