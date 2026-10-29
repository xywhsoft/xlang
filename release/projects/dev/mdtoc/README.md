# mdtoc - Markdown 目录 (TOC) 生成器

扫描 Markdown 标题行（`#`~`######`）生成目录，并插入到文件首个 `---`（frontmatter 结束符或分隔线）或空行之后。

## 用法

```
mdtoc <file.md>                       生成 TOC 并写入文件
mdtoc <file.md> -o                    只把 TOC 输出到 stdout，不改文件
mdtoc <file.md> --min-level 2         最小标题级别（默认 1）
mdtoc <file.md> --max-level 3         最大标题级别（默认 6）
```

选项可组合，例如：

```
mdtoc <file.md> --min-level 2 --max-level 3 -o
```

## TOC 格式

```
## 目录
- [标题1](#标题1)
  - [子标题](#子标题)
- [标题2](#标题2)
```

- 缩进按 `(level - minLevel) * 2` 空格递进。
- anchor：小写化、空格转 `-`、移除其它特殊 ASCII 字符（保留字母、数字、CJK）。
- 标题识别：行首 `#` 数量为级别（1~6），`#` 后须跟空格。

## 运行与构建

```bat
cd /d D:\GIT\x-lang\release

REM 直接运行
xl run projects\dev\mdtoc\mdtoc.xl -- projects\dev\mdtoc\test.md -o

REM 构建独立 exe
xl build projects\dev\mdtoc\mdtoc.xl -o projects\dev\mdtoc\mdtoc.exe
projects\dev\mdtoc\mdtoc.exe projects\dev\mdtoc\test.md -o
```

## 实现说明

- 用 `string.splitLines` 切行、`string.trim`/`startsWith`/`charAt` 识别标题、`string.lower`/`chars`/`byteAt` 生成 anchor。
- 标题级别与文本存入平行动态数组；TOC 行用 `string.repeat` 生成缩进。
- 插入点：若文件首行为 `---`（frontmatter），定位闭合 `---` 之后；否则取首个空行或 `---` 之后；都找不到则插到开头。
- 跳过标题为「目录」的 `##` 行，避免重复运行时把自身的 TOC 标题作为条目。

## 已知限制 / 绕过

- **不替换已有 TOC 块**：重复运行会在新插入点追加 TOC，不会清除旧块。需要时先手工删除旧的 `## 目录` 段再运行。
- **`int()` 对非数字串不抛异常**（见 `BUGS.md` BUG-M1）：`--min-level abc` 不会报错，而是被当成 0 然后归一化为 1。当前用 try/catch 兜底但无法捕获，故 `--min-level/--max-level` 不会因非法值退出非零码。
- **BUG-S1 绕过**：`string.splitLines` 在末尾换行时会多出一个尾部空串，本工具用「有效长度」跳过该幻影行，不对返回数组做 `remove`。
