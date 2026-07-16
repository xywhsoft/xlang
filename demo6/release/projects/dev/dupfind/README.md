# dupfind - 重复文件检测器

按“文件大小 + SHA-256”查找内容重复的文件，展示目录遍历、二进制文件流、流式摘要和类型化容器的组合使用。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\dupfind\dupfind.xl -- <目录> [-r]
```

| 参数 | 作用 |
|---|---|
| `<目录>` | 扫描目标；省略时使用当前目录 |
| `-r` / `--recursive` | 递归扫描子目录 |
| `-h` / `--help` | 显示帮助 |

## 算法

1. `dir.entries` 收集普通文件路径和大小。
2. 使用 `dict<int>` 在线统计大小桶，大小唯一的文件不计算摘要。
3. 对候选文件使用 `File` 以二进制方式打开，每次读取 64 KiB `Buffer` 并更新 `Sha256`。
4. 使用 `(size, digest)` 组成分组键，仅输出至少包含两个文件的组。

哈希过程直接处理原始字节，不把二进制数据转换成文本，也不会把整个大文件一次性读入内存。

## 构建与验证

```bat
xl build projects\dev\dupfind\dupfind.xl -o projects\dev\dupfind\dupfind.exe
projects\dev\dupfind\dupfind.exe projects\dev\dupfind\testdata -r
```

受控测试目录应输出 2 个重复组、4 个可去重文件；同大小但内容不同的文件不会被误报。
