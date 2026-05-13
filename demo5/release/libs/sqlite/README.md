# sqlite xlang package

低级 SQLite 扩展库，随包携带官方 SQLite amalgamation 源码并在构建时编译。

接口尽量保持 SQLite C API 的操作手感：

- `sqlite3_open` / `sqlite3_open_v2` 返回 `Db` handle。
- `sqlite3_prepare_v2` 返回 `Stmt` handle。
- `sqlite3_step` 返回 `SQLITE_ROW` 或 `SQLITE_DONE`。
- `sqlite3_bind_*` 使用 SQLite 的 1-based 参数索引。
- `sqlite3_column_*` 使用 SQLite 的 0-based 列索引。
- `sqlite3_finalize` / `sqlite3_close_v2` 可显式释放；`Db` 和 `Stmt` 析构时也会兜底释放。

受 xlang 当前 FFI 类型约束，少量 out 参数 API 改为返回 handle；`sqlite3_exec` 不支持回调，适合执行 DDL/DML。查询请使用 `prepare/step/column/finalize`。
