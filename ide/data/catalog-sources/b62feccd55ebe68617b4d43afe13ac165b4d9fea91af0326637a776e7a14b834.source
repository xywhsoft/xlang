# MySQL 协议与文本辅助

本包目前不是 MySQL 数据库驱动：没有连接、握手、认证、TLS、查询、预处理语句、
结果集或事务会话。以下八个辅助接口可以独立使用；不能凭包名推断完整数据库能力。

## 公开 API

| xlang 接口 | 当前合同 |
| --- | --- |
| `cmdName(int code) as string` | 经典协议 0..31 命令名称；其他值为 `UNKNOWN`。名称不代表目标服务器仍支持该命令。 |
| `colType(int code) as string` | 当前列出的经典列类型；未知编号为 `UNKNOWN`。不是完整版本协商。 |
| `statusStr(int flags) as string` | 按位升序输出 14 个已知状态标志，以空格分隔；没有已知位返回空字符串，未知位忽略。 |
| `capName(int flag) as string` | 查询单个已登记能力位；未登记位或组合值为 `UNKNOWN`。 |
| `escape(string text) as string` | 转义引号、反斜杠、换行、回车和 0x1a；不做参数绑定。 |
| `formatDsn(string host, int port, string dbname, string user) as string` | 生成 `mysql://user@host:port/dbname`；含冒号的原始 host 自动包方括号，port 必须为 1..65535。 |
| `dsnPart(string dsn, string part) as string` | 从上述 DSN 取 `user`、`host`、`port` 或 `db`；host 不含方括号。 |
| `errorClass(int code) as string` | 经典范围粗分类：1..999 为 global，2000..2999 为 client，1000..1999/3000..4999 为 server，其他为 unknown。不是每个编号存在性的验证。 |

`capName` 当前登记 LONG_PASSWORD、FOUND_ROWS、LONG_FLAG、CONNECT_WITH_DB、
PROTOCOL_41、SECURE_CONNECTION、PLUGIN_AUTH、CONNECT_ATTRS、PLUGIN_AUTH_LENENC。
`statusStr` 包括 IN_TRANS、AUTOCOMMIT、MORE_RESULTS、NO_GOOD_INDEX、NO_INDEX、
CURSOR_EXISTS、LAST_ROW_SENT、DB_DROPPED、NO_BACKSLASH_ESCAPES、METADATA_CHANGED、
QUERY_WAS_SLOW、PS_OUT_PARAMS、IN_TRANS_READONLY、SESSION_STATE_CHANGED。

## 文本与所有权

- 文本是 UTF-8、NUL 结尾字符串；不能借这些接口传递内嵌 NUL 的二进制数据。
- C 头是 `include/mysql_native.h`。查表结果是只读常量；escape、statusStr、DSN
  结果借用当前 XRT 临时区。连续调用不会互相覆盖，不可单独 `free`。
  C 调用方在自己的临时区生命周期结束前复制需长期保留的内容。
- `escape`、`formatDsn`、`dsnPart` 另有同名 `_owned` C 入口，返回独立的
  XRT 分配，调用方用 `xrtFree` 释放。XLang 使用拥有型入口并直接接管结果，
  避免临时区结果再次复制为语言字符串；原 C 借用入口保持不变。
- 不使用固定大小共享字符串缓冲区，不静默截断。分配失败保留 XRT 内存错误。
- 参数非法走 XRT argument 错误，xlang 对应 `ValueError`；不能把错误当空结果。
- DSN 支持 UTF-8 user/db 和未加方括号的 IPv6 host，但不处理密码、查询参数、
  百分号编解码或主机地址有效性。会改变结构的分隔符及空白/控制字符被拒绝；
  user/db 可为空，host 不可为空。
- `escape` **不适用于 NO_BACKSLASH_ESCAPES 模式**，也不提供连接字符集感知转义。
  它不能代替数据库驱动的参数化查询，不应据此拼接不受信任的 SQL。

## 范例与验证

`release/examples/extlibs/mysql.xl` 与包内 `examples/basic.c` 实现相同功能，
固定输出见 `examples/basic.expected.txt`。不需要数据库服务。

在 demo6 根目录执行 `powershell -File tools/check_extlib_helper_boundaries.ps1`；
Linux 执行 `bash tools/check_extlib_helper_boundaries.sh`。
门禁覆盖 16384 个状态组合、命令/能力/错误范围抽查、长字符串、多次返回值并存、
UTF-8、DSN/端口错误和恢复，并包含 FAISS 标量距离辅助回归。
这不是数据库客户端准入、全部协议覆盖、并发或分配故障验证。

协议参考：[命令枚举](https://dev.mysql.com/doc/dev/mysql-server/8.4.9/my__command_8h_source.html)、
[状态与能力位](https://dev.mysql.com/doc/dev/mysql-server/latest/mysql__com_8h.html)、
[客户端错误范围](https://dev.mysql.com/doc/dev/mysql-server/8.4.9/errmsg_8h_source.html)。
