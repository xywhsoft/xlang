# Closure 与 continuation scope 门禁

共享测试 DLL 的静态默认值仍使用旧位置字段，新增 `program` 后被原严格
GCC 检查拒绝。改为 `.abi=2, .i64_value=42` 的具名常量描述，其余字段按 C
规则清零；不抑制警告，也不修改运行时描述结构或默认值语义。

两个工具的实际原生依赖改由现有 IR／驻留注册表的规范闭包选取，GCC 使用
响应文件；补齐正式头文件路径与真实 C 字符串 define 传输。所有原生 DLL、
TCC、语言运行、重复发射及 allocator／OOM 见证保留。

固定 R8 完整源码 cohort 的结果：

- `continuation-scopes-r9` 全部 4 配置通过，覆盖 O0/O2 原生及生成语言 TCC。
  原生配置各 100 个真实 DLL 生命周期、200 对 run/release、350 个继承 closure、
  100 次无分配后继 abort／recommit、25 次四线程 drain、100 次实际根 lease
  OOM 拒绝及 100 次容量回滚。语言配置各 100 次实际 TCC 生命周期、50 个
  Future 持有及 50 个提取引用，代码映射与精确内存平衡均检查。
- `resident-closure-r10` 两个完整原生配置、20 个语言阶段通过。
  保留 50 个不同 A/B DLL 调用、25 个双 owner 循环转换、100000 对 retain／
  release、实际 OOM 拒绝与 24 个 IR 拒绝变异；重复 C／头文件／IR 一致。

独立重算 7322 项输入、工具、产物及日志哈希：
`F:/xlang-validation/five-groups-20261027/p7-shipping/closure-scope-r10.proof.json`。
这些是两个完整 worker 的证据，不能替代仍在运行的全部参数化门禁结果。
