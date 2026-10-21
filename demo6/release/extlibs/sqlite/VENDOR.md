# SQLite 来源记录

- 随包源码：`third_party/sqlite3.c`、`sqlite3.h`、`sqlite3ext.h`。
- 源码声明版本：`3.53.1`，版本号 `3053001`。
- 源码声明 Source ID：`2026-05-05 10:34:17 c88b22011a54b4f6fbd149e9f8e4de77658ce58143a1af0e3785e4e6475127e9`。
- 上游：[SQLite](https://www.sqlite.org/)。源码头声明为 public domain，保留原始声明。
- 本批没有升级或修改上述三个第三方文件。门禁记录各文件 SHA256，版本信息来自
  当前仓库源码，不表示本批重新下载并逐字比对了上游发布归档。

产品保留上游默认配置；测试中的分配故障使用 `sqlite3_config`，仅影响独立测试
进程，不改变产品的全局分配器设置。
