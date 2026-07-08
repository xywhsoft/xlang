# duptop - disk usage top

扫描目录, 按大小降序输出 TopN 最大文件 / 目录.

## 用法

```
duptop <path> [N=10]     扫描 path 下文件, 按大小降序输出 TopN
duptop <path> -d         只统计目录大小 (每个子目录用 dir.size 汇总)
duptop <path> -r         递归扫描所有子目录中的文件
duptop <path> -d -r      递归列出所有层级目录及其累计大小
duptop <path> -n 20      显式指定 TopN
duptop                   默认当前目录, N=10
```

## 示例

```
> duptop examples 10 -r
Top 10 largest files in: examples
──────────────────────────────────────────────────
 1.   223.0 KB  task_api.xl
 2.   128.9 KB  feature_suite\11_concurrency.xl
 ...
Total: 172 files, 855.7 KB
```

## 设计

- `dir.entries(path, recursive)` 返回的 var 字典数组已含 `size`/`path`/`isFile`/`isDir`, 文件模式直接读 `size`, 无需 `file.info`.
- **B2 绕过**: array 无 append, 用 `list<string>` + `list<int64>` 平行收集, 越界索引赋值扩容 (与 primes 同款手法).
- **部分选择排序**: 只跑 `min(N, count)` 轮把最大的 N 个换到队首, O(N*count), 避免对全量排序.
- 目录模式用 `dir.size(path)` 重新汇总; 路径显示用 `path.rel` 转 root 相对路径.
- `int64` 承载字节数, 避免大目录累计溢出 32 位 int.

## 验证

两条路径 (xl run / built exe) 均通过, ASCII argv 下输出一致, EXIT=0. 详见 BUGS.md NOTE-010.
