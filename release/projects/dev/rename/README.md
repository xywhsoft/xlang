# rename - 文件批量重命名工具

批量改名工具，默认 **dry-run 预览**，加 `--do` 才实际改名。

## 用法

```
rename ext <dir> <old_ext> <new_ext> [--do]   批量改扩展名（old_ext 不含点也可）
rename prefix <dir> <prefix> [--do]           给每个文件名加前缀
rename seq <dir> <pattern> <start> [--do]     序列重命名，保留原扩展名
rename lower <dir> [--do]                     文件名转小写
rename replace <dir> <from> <to> [--do]       替换文件名中的字符串
rename -h | --help | help                     帮助
```

- 不带 `--do` 时只打印"将要做什么"，不改动任何文件；`--do` 可放在子命令后任意位置。
- 用 `dir.files(dir, false)` 列顶层文件（非递归），`file.rename(old, new, false)` 改名（不覆盖）。
- 文件按路径大小写敏感升序排序后处理，保证 `seq` 编号跨平台/跨运行稳定。
- 扩展名匹配大小写不敏感（`.txt` 命中 `Delta.TXT`）；`ext` 仅替换匹配 `old_ext` 的文件，其余跳过。

### seq pattern 语法

- `{000}` → 序号零填充到宽度 3（宽度 = 花括号内字符数）
- `{n}` → 宽度 1（无可见填充）
- 无花括号 → 序号直接追加到 pattern 末尾
- **原扩展名总是保留**（只替换基础名）

示例：

```
rename seq . file_{000} 1 --do   ->   file_001.txt, file_002.txt, ...
rename seq . img_{n} 1 --do      ->   img_1.txt, img_2.txt, ...
rename seq . item 5 --do         ->   item5.txt, item6.txt, ...
```

## 验证

```bat
cd /d D:\GIT\x-lang\release
xl run  projects\dev\rename\rename.xl -- lower projects\dev\rename\testdata
xl build projects\dev\rename\rename.xl -o projects\dev\rename\rename.exe
```

两条路径（`xl run` 与 built exe）均 `EXIT=0`，输出一致。testdata 含 7 个文件
（含大小写混合名 `Delta.TXT`/`MIXED.txt`/`Ph0t01.jpg`，专门覆盖 `lower` 与扩展名匹配）。

## 跨平台大小写重命名

在大小写不敏感文件系统上，`Delta.TXT -> delta.txt` 的目标路径会被系统视为原文件。
`commitRename` 对这种情况使用同目录临时名完成两步原子迁移；第二步失败时尝试回滚。

文件列表使用 `array<string>`，直接进行确定性的区分大小写排序，再按顺序执行或预览重命名。
