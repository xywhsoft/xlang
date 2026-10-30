# Plist 双语状态保存范例

从仓库根目录运行，不需要文件、网络或其他服务：

```powershell
./release/xl.exe run release/extlibs/plist/examples/portable_state.xl
./release/xl.exe run release/extlibs/plist/examples/portable_state_c.c
```

Linux 将 `./release/xl.exe` 换成 `./release/xl`。两份程序使用同一份
中文玩家状态，先以 XML 保存普通字段，再用 binary 保存 UID/null，
分别读回核对类型和值。随后验证“不支持的 XML 值”为 TYPE(4)，
binary 条目超限为 LIMIT(5)，并在失败后重新解析原输入。

固定输出由 `portable_state.expected.txt` 保存：

```text
player=阿青; score=42
encoding=xml,binary; uid=42; checkpoint=null
type rejected=true; entry limit=true; recovery=true
plist portable state: OK
```

C 使用公开的 xrtPlist* API，不构造 XLang 值；节点属于 document，
查询返回的节点仅借用，不逐节点释放。程序统一释放文档、XML/binary
缓冲，错误出口也不丢弃已持有结果。XLang 返回普通值和独立 Buffer，
由语言生命周期管理。两种表层的构树/转换成本不同，不声称零开销。

这个 C 单文件包含包内核心与 XML/Expat 实现以支持 `xl run`，独立
GCC 构建时只再链接一份 XRT；不得把已经包含的实现重复编译链接。
默认包门禁分别核对 GCC、C/XLang JIT/AOT 和黄金输出，不只对比两份
程序是否输出同一个错误结果。

原有更完整的配置/文件互通例在 `release/examples/extlibs/plist.xl`
和 `plist_c.c`。它们的可选路径参数可保留 XML 与 binary 产物，并由
`test/check_external.py` 使用 Python plistlib 独立读取；新例专注
内存中的状态转换，没有外部资源和清理目录需求。
