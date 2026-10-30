# Sitemap 与 robots 双语范例

命令从仓库根目录执行；Windows 用 `release/xl.exe`，Linux 用
`release/xl`。包依赖 XML SAX/Expat，多源 C 例通过包级工具或门禁编译，
不是把 XML 解析器复制进一个单文件范例。

## 规则分组与决策

`robots_policy.xl` 与 `robots_policy_c.c` 使用同一份 CR 换行规则，展示：

- 注释不会结束当前规则组。
- 较长的 Allow 规则覆盖较短的 Disallow。
- 未命中专属代理组时使用 `*`，但专属组不会叠加通配组规则。
- 解析结果同时保留 Sitemap 声明，C 调用方最终释放整个 `xvalue`。

```powershell
release/xl.exe run release/extlibs/sitemap/examples/robots_policy.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package sitemap -Source release/extlibs/sitemap/examples/robots_policy_c.c
release/xl.exe build release/extlibs/sitemap/examples/robots_policy.xl -o build/test/sitemap_policy.exe
build/test/sitemap_policy.exe
```

输出固定为 `robots_policy.expected.txt` 的四行：

```text
draft: false
published: true
fallback: false
specific: true
```

不访问网络、不写文件。包门禁对 C 的 GCC、TCC JIT/AOT 和 XLang JIT/AOT
逐一核对该输出；门禁生成的程序、对象和报告只在隔离测试目录内。

## XML 生成与读取

`sitemap_c.c` 与中央 `release/examples/extlibs/sitemap_pair.xl` 原有目标
保留：生成 URL 集合与索引、解析回读、robots 决策，覆盖六个语言入口。
两份 XML 由 Python 标准库独立核对命名空间、实体解码和 URL。

当前原生 C 生成/读取与范例通过，语言字面量数组传入生成入口仍受
共享 M1 投影问题阻断。不能拿只读 robots 范例的成功替代这条生成路径，
也不应在包里识别编译器私有容器句柄绕过。完整门禁维持失败预期为
“应成功而实际失败”，修复主线后原样重跑。

完整范围与证据见包 README 及 `ROBOTS_NEWLINES_AUDIT_2026-10-30.md`。
